// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mt6993-fmeter.h"

enum fm_sys_id {
	FM_CKSYS_TOP = 0,
	FM_CKSYS_MM,
	FM_CKSYS_VLP,
	FM_CKSYS_TOP_CKMTR,
	FM_CKSYS_MM_CKMTR,
	FM_CKSYS_VLP_CKMTR,
	FM_CLKSQR,
	FM_MAINPLL_CTRL,
	FM_UNIVPLL_CTRL,
	FM_MSDCPLL_CTRL,
	FM_EMIPLL_CTRL,
	FM_MAINPLL2_CTRL,
	FM_UNIVPLL2_CTRL,
	FM_MMPLL_CTRL,
	FM_IMGPLL_CTRL,
	FM_TVDPLL_CTRL,
	FM_APLL1_CTRL,
	FM_APLL2_CTRL,
	FM_CCIPLL,
	FM_PTPPLL,
	FM_SYS_NUM,
};

#define FM_TIMEOUT			30

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static void __iomem *fm_base[FM_SYS_NUM];

const char *comp_list[] = {
	[FM_CKSYS_TOP] = "mediatek,mt6993-cksys_top",
	[FM_CKSYS_MM] = "mediatek,mt6993-cksys_mm",
	[FM_CKSYS_VLP] = "mediatek,mt6993-cksys_vlp",
	[FM_CKSYS_TOP_CKMTR] = "mediatek,mt6993-cksys_top_ckmtr",
	[FM_CKSYS_MM_CKMTR] = "mediatek,mt6993-cksys_mm_ckmtr",
	[FM_CKSYS_VLP_CKMTR] = "mediatek,mt6993-cksys_vlp_ckmtr",
	[FM_CLKSQR] = "mediatek,mt6993-clksq_ctrl",
	[FM_MAINPLL_CTRL] = "mediatek,mt6993-mainpll_ctrl",
	[FM_UNIVPLL_CTRL] = "mediatek,mt6993-univpll_ctrl",
	[FM_MSDCPLL_CTRL] = "mediatek,mt6993-msdcpll_ctrl",
	[FM_EMIPLL_CTRL] = "mediatek,mt6993-emipll_ctrl",
	[FM_MAINPLL2_CTRL] = "mediatek,mt6993-mainpll2_ctrl",
	[FM_UNIVPLL2_CTRL] = "mediatek,mt6993-univpll2_ctrl",
	[FM_MMPLL_CTRL] = "mediatek,mt6993-mmpll_ctrl",
	[FM_IMGPLL_CTRL] = "mediatek,mt6993-imgpll_ctrl",
	[FM_TVDPLL_CTRL] = "mediatek,mt6993-tvdpll_ctrl",
	[FM_APLL1_CTRL] = "mediatek,mt6993-apll1_ctrl",
	[FM_APLL2_CTRL] = "mediatek,mt6993-apll2_ctrl",
	[FM_CCIPLL] = "mediatek,mt6993-ccipll_pll_ctrl",
	[FM_PTPPLL] = "mediatek,mt6993-ptppll_pll_ctrl",
};

#define CLKMON(_n, _t, _i, _o, _b, _c, _f, _chk) {.name = _n, \
	.type = _t, .id = _i, .fenc_ofs = _o, .fenc_bit = _b, .ck_div = _c, .target_freq = _f, .need_check = _chk, .is_pll = 0 }

#define PLL_MON(_n, _t, _i, _c, _f, _d, _chk) {.name = _n, \
	.type = _t, .id = _i, .ck_div = _c, .target_freq = _f, .domain = _d, .need_check = _chk, .is_pll = 1 }

struct fmeter_clk fclks_arr[] = {
	CLKMON("hf_faxi_ck", CKGEN, 1, 0x0270, 31, 1, 156000, 1),
	CLKMON("hf_fperi_axi_ck", CKGEN, 2, 0x0270, 30, 1, 156000, 1),
	CLKMON("hf_fch_infra_axi_ck", CKGEN, 3, 0x0270, 29, 1, 156000, 1),
	CLKMON("hf_fch_infra_ck", CKGEN, 4, 0x0270, 28, 1, 218400, 1),
	CLKMON("hf_fmem_sub_ck", CKGEN, 5, 0x0270, 27, 1, 546000, 1),
	CLKMON("hf_fhash_sub_ck", CKGEN, 6, 0x0270, 26, 1, 546000, 1),
	CLKMON("hf_fperi_fmem_sub_ck", CKGEN, 7, 0x0270, 25, 1, 546000, 1),
	CLKMON("hf_fzram_sub_ck", CKGEN, 8, 0x0270, 24, 1, 546000, 1),
	CLKMON("hf_fio_noc_ck", CKGEN, 9, 0x0270, 23, 1, 242700, 1),
	CLKMON("hf_fhash_noc_ck", CKGEN, 10, 0x0270, 22, 1, 728000, 1),
	CLKMON("hf_fperi_noc_ck", CKGEN, 11, 0x0270, 21, 1, 728000, 1),
	CLKMON("hf_femi_interface_546_ck", CKGEN, 12, 0x0270, 20, 1, 546000, 1),
	CLKMON("hf_femi_n_ck", CKGEN, 13, 0x0270, 19, 1, 218400, 1),
	CLKMON("hf_femi_s_ck", CKGEN, 14, 0x0270, 18, 1, 218400, 1),
	CLKMON("hf_femi_infra_ck", CKGEN, 15, 0x0270, 17, 1, 312000, 1),
	CLKMON("hf_femi_infra_sspm_ck", CKGEN, 16, 0x0270, 16, 1, 364000, 1),
	CLKMON("f_fulposc_emi_infra_ck", CKGEN, 17, 0x0270, 15, 1, 260000, 1),
	CLKMON("f_femi_infra_26m_ck", CKGEN, 18, 0x0270, 14, 1, 26000, 1),
	CLKMON("f_fch_infra_sys_26m_ck", CKGEN, 19, 0x0270, 13, 1, 26000, 1),
	CLKMON("hf_fcbus_phy_ck", CKGEN, 20, 0x0270, 12, 1, 218400, 1),
	CLKMON("hf_fatb_ck", CKGEN, 21, 0x0270, 11, 1, 364000, 1),
	CLKMON("hf_fcirq_ck", CKGEN, 22, 0x0270, 10, 1, 78000, 1),
	CLKMON("hf_fmcu_infra_ck", CKGEN, 23, 0x0270, 9, 1, 242700, 1),
	CLKMON("hf_fmcupm_ck", CKGEN, 24, 0x0270, 8, 1, 218400, 1),
	CLKMON("hf_fsdf_ck", CKGEN, 25, 0x0270, 7, 1, 624000, 1),
	CLKMON("hf_fmfg_eb_ck", CKGEN, 26, 0x0270, 6, 1, 218400, 1),
	CLKMON("hf_fapu_ext_ck", CKGEN, 27, 0x0270, 5, 1, 173300, 1),
	CLKMON("f_fap2conn_host_ck", CKGEN, 28, 0x0270, 4, 1, 78000, 1),
	CLKMON("hf_fssr_pka_ck", CKGEN, 29, 0x0270, 3, 1, 436800, 1),
	CLKMON("hf_fssr_dma_ck", CKGEN, 30, 0x0270, 2, 1, 436800, 1),
	CLKMON("hf_fssr_kdf_ck", CKGEN, 31, 0x0270, 1, 1, 312000, 1),
	CLKMON("hf_fssr_rng_ck", CKGEN, 32, 0x0270, 0, 1, 273000, 1),
	CLKMON("hf_fdxcc_ck", CKGEN, 33, 0x0274, 31, 1, 68300, 1),
	CLKMON("f_fefuse_ck", CKGEN, 34, 0x0274, 30, 1, 26000, 1),
	CLKMON("f_fmem_dly_ip_ck", CKGEN, 35, 0x0274, 29, 1, 173300, 1),
	CLKMON("f_fdpsw_cmp_26m_ck", CKGEN, 36, 0x0274, 28, 1, 26000, 1),
	CLKMON("f_fadsp_uarthub_bclk_ck", CKGEN, 37, 0x0274, 27, 1, 104000, 1),
	CLKMON("hf_faud_1_ck", CKGEN, 38, 0x0274, 26, 1, 180600, 1),
	CLKMON("hf_faud_2_ck", CKGEN, 39, 0x0274, 25, 1, 196600, 1),
	CLKMON("hf_fdpmaif_main_ck", CKGEN, 40, 0x0274, 24, 1, 273000, 1),
	CLKMON("f_fipseast_ck", CKGEN, 41, 0x0274, 23, 1, 436800, 1),
	CLKMON("f_fipswest_ck", CKGEN, 42, 0x0274, 22, 1, 436800, 1),
	CLKMON("hf_fsmapck_ck", CKGEN, 43, 0x0274, 21, 1, 68300, 1),
	CLKMON("hf_fipic_ck", CKGEN, 44, 0x0274, 20, 1, 156000, 1),
	CLKMON("hf_fspi0_bclk_ck", CKGEN, 45, 0x0274, 19, 1, 208000, 1),
	CLKMON("hf_fspi1_bclk_ck", CKGEN, 46, 0x0274, 18, 1, 208000, 1),
	CLKMON("hf_fspi2_bclk_ck", CKGEN, 47, 0x0274, 17, 1, 208000, 1),
	CLKMON("hf_fspi3_bclk_ck", CKGEN, 48, 0x0274, 16, 1, 208000, 1),
	CLKMON("hf_fspi4_bclk_ck", CKGEN, 49, 0x0274, 15, 1, 208000, 1),
	CLKMON("hf_fspi5_bclk_ck", CKGEN, 50, 0x0274, 14, 1, 208000, 1),
	CLKMON("hf_fspi6_bclk_ck", CKGEN, 51, 0x0274, 13, 1, 208000, 1),
	CLKMON("hf_fspi7_bclk_ck", CKGEN, 52, 0x0274, 12, 1, 208000, 1),
	CLKMON("f_fpextp_mbist_ck", CKGEN, 53, 0x0274, 11, 1, 499200, 1),
	CLKMON("hf_ftl_ck", CKGEN, 54, 0x0274, 10, 1, 136500, 1),
	CLKMON("hf_ftl_p1_ck", CKGEN, 55, 0x0274, 9, 1, 218400, 1),
	CLKMON("hf_fpwm_ck", CKGEN, 56, 0x0274, 8, 1, 26000, 1),
	CLKMON("hf_faes_ufsfde_0_ck", CKGEN, 57, 0x0274, 7, 1, 546000, 1),
	CLKMON("hf_fufs_0_ck", CKGEN, 58, 0x0274, 6, 1, 499200, 1),
	CLKMON("f_fufs_mbist_0_ck", CKGEN, 59, 0x0274, 5, 1, 624000, 1),
	CLKMON("hf_faes_ufsfde_1_ck", CKGEN, 60, 0x0274, 4, 1, 546000, 1),
	CLKMON("hf_fufs_1_ck", CKGEN, 61, 0x0274, 3, 1, 499200, 1),
	CLKMON("f_fufs_mbist_1_ck", CKGEN, 62, 0x0274, 2, 1, 624000, 1),
	CLKMON("f_fuarthub_bclk_ck", CKGEN, 63, 0x0274, 1, 1, 26000, 1),
	CLKMON("f_fuart_ck", CKGEN, 64, 0x0274, 0, 1, 26000, 1),
	CLKMON("f_fi2c_peri_ck", CKGEN, 65, 0x0278, 31, 1, 124800, 1),
	CLKMON("f_fi2c_north_ck", CKGEN, 66, 0x0278, 30, 1, 124800, 1),
	CLKMON("f_fi2c_east_ck", CKGEN, 67, 0x0278, 29, 1, 124800, 1),
	CLKMON("f_fi2c_west_ck", CKGEN, 68, 0x0278, 28, 1, 124800, 1),
	CLKMON("hf_fmsdc_macro_1p_ck", CKGEN, 69, 0x0278, 27, 1, 416000, 1),
	CLKMON("hf_fmsdc_macro_2p_ck", CKGEN, 70, 0x0278, 26, 1, 416000, 1),
	CLKMON("hf_fmsdc30_1_ck", CKGEN, 71, 0x0278, 25, 1, 208000, 1),
	CLKMON("hf_fmsdc30_2_ck", CKGEN, 72, 0x0278, 24, 1, 208000, 1),
	CLKMON("hf_fcksys_mm_mainpll_d3_ck", CKGEN, 73, 0x0278, 23, 1, 728000, 1),
	CLKMON("hf_fcksys_mm_mainpll_d4_ck", CKGEN, 74, 0x0278, 22, 1, 546000, 1),
	CLKMON("hf_fcksys_mm_mainpll_d5_ck", CKGEN, 75, 0x0278, 21, 1, 436800, 1),
	CLKMON("hf_fcksys_mm_mainpll_d7_ck", CKGEN, 76, 0x0278, 20, 1, 312000, 1),
	CLKMON("hf_fcksys_vlp_mainpll_d4_ck", CKGEN, 77, 0x0278, 19, 1, 546000, 1),
	CLKMON("hf_fcksys_vlp_mainpll_d5_ck", CKGEN, 78, 0x0278, 18, 1, 436800, 1),
	CLKMON("hf_fcksys_vlp_mainpll_d6_ck", CKGEN, 79, 0x0278, 17, 1, 364000, 1),
	CLKMON("hf_fcksys_vlp_mainpll_d7_ck", CKGEN, 80, 0x0278, 16, 1, 312000, 1),
	CLKMON("hf_fcksys_vlp_mainpll_d9_ck", CKGEN, 81, 0x0278, 15, 1, 242700, 1),
	CLKMON("f_fgridsensor_ck", CKGEN, 82, 0x0278, 14, 1, 26000, 1),
	CLKMON("f_faov_26m_ck", CKGEN, 83, 0x0278, 13, 1, 26000, 1),
	CLKMON("hf_femi_wdat_ck", CKGEN, 84, 0x0278, 12, 1, 273000, 1),
	CLKMON("clksq_26m_ck", ABIST, 1, 0xdead, 0xdead, 1, 26000, 1),
	CLKMON("rtc_32k_ck", ABIST32K, 2, 0xdead, 0xdead, 1, 32, 1),
	PLL_MON("mainpll_ckdiv_ck", ABIST, 3, 1, 2184000, MAINPLL_DB, 1),
	PLL_MON("univpll_ckdiv_ck", ABIST, 4, 1, 2496000, UNIVPLL_DB, 1),
	PLL_MON("emipll_ckdiv_ck", ABIST, 5, 1, 2880000, EMIPLL_DB, 1),
	PLL_MON("msdcpll_ckdiv_ck", ABIST, 6, 4, 416000, MSDCPLL_DB, 1),
	CLKMON("mcusys_arm_clk_out_all_ck_out", ABIST, 11, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("dsi1_lntc_dsiclk_fqmtr_ck", ABIST, 13, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("ad_dsi1_mppll_tst_ck", ABIST, 14, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("dsi02_lntc_dsiclk_fqmtr_ck", ABIST, 15, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("ad_dsi02_mppll_tst_ck", ABIST, 16, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("ufs_mp_clk2freq_ck", ABIST, 19, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("gb_hd_femi_ck_1_", ABIST, 24, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("top_r0_out_fm", ABIST, 25, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("gb_hd_femi_ck_0_", ABIST, 26, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("sth_hd_fmem2_ck", ABIST, 27, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("nth_hd_fmem2_ck", ABIST, 28, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("pextp_phy_clk_to_freqmeter_p1", ABIST, 30, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("pextp_phy_clk_to_freqmeter_p0", ABIST, 31, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("hf_fmmup_ck", CKGEN_CK2, 1, 0x0270, 31, 1, 728000, 1),
	CLKMON("f_fmminfra_ao_ck", CKGEN_CK2, 2, 0x0270, 30, 1, 728000, 1),
	CLKMON("f_fmminfra_ck", CKGEN_CK2, 3, 0x0270, 29, 1, 624000, 1),
	CLKMON("f_fmminfra_snoc_ck", CKGEN_CK2, 4, 0x0270, 28, 1, 832000, 1),
	CLKMON("hf_fvenc_ck", CKGEN_CK2, 5, 0x0270, 27, 1, 687500, 1),
	CLKMON("hf_fvenc_mdp_ck", CKGEN_CK2, 6, 0x0270, 26, 1, 687500, 1),
	CLKMON("hf_fvdec_ck", CKGEN_CK2, 7, 0x0270, 25, 1, 546000, 1),
	CLKMON("hf_fimg1_ck", CKGEN_CK2, 8, 0x0270, 24, 1, 660000, 1),
	CLKMON("hf_fipe_ck", CKGEN_CK2, 9, 0x0270, 23, 1, 728000, 1),
	CLKMON("hf_fdisp_ck", CKGEN_CK2, 10, 0x0270, 22, 1, 832000, 1),
	CLKMON("hf_fmml_ck", CKGEN_CK2, 11, 0x0270, 21, 1, 624000, 1),
	CLKMON("hf_fdvo_dp_ck", CKGEN_CK2, 12, 0x0270, 20, 1, 297000, 1),
	CLKMON("hf_fdvo_favt_dp_ck", CKGEN_CK2, 13, 0x0270, 19, 1, 297000, 1),
	CLKMON("hf_fcam_ck", CKGEN_CK2, 14, 0x0270, 18, 1, 26000, 1),
	CLKMON("f_fcamtm_ck", CKGEN_CK2, 15, 0x0270, 17, 1, 208000, 1),
	CLKMON("hf_fccusys_ck", CKGEN_CK2, 16, 0x0270, 16, 1, 832000, 1),
	CLKMON("f_fccutm_ck", CKGEN_CK2, 17, 0x0270, 15, 1, 208000, 1),
	CLKMON("f_fseninf0_ck", CKGEN_CK2, 18, 0x0270, 14, 1, 312000, 1),
	CLKMON("f_fseninf1_ck", CKGEN_CK2, 19, 0x0270, 13, 1, 312000, 1),
	CLKMON("f_fseninf2_ck", CKGEN_CK2, 20, 0x0270, 12, 1, 312000, 1),
	CLKMON("f_fseninf3_ck", CKGEN_CK2, 21, 0x0270, 11, 1, 312000, 1),
	CLKMON("f_fseninf4_ck", CKGEN_CK2, 22, 0x0270, 10, 1, 312000, 1),
	CLKMON("f_fseninf5_ck", CKGEN_CK2, 23, 0x0270, 9, 1, 312000, 1),
	CLKMON("f_fmminfra_snoc_slow_ck", CKGEN_CK2, 24, 0x0270, 8, 1, 624000, 1),
	CLKMON("hf_fcksys_top_mmpll_d2_ck", CKGEN_CK2, 25, 0x0270, 7, 1, 1375000, 1),
	PLL_MON("mainpll2_ckdiv_ck", ABIST_CK2, 1, 1, 2184000, MAINPLL2_DB, 1),
	PLL_MON("univpll2_ckdiv_ck", ABIST_CK2, 2, 1, 192000, UNIVPLL2_DB, 1),
	PLL_MON("mmpll_ckdiv_ck", ABIST_CK2, 3, 1, 2750000, MMPLL_DB, 1),
	PLL_MON("imgpll_ckdiv_ck", ABIST_CK2, 4, 1, 2640000, IMGPLL_DB, 1),
	PLL_MON("tvdpll_ckdiv_ck", ABIST_CK2, 5, 4, 594000, TVDPLL_DB, 1),
	CLKMON("ad_csi0a_dphy_delaycal_ck", ABIST_CK2, 8, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("ad_csi0b_dphy_delaycal_ck", ABIST_CK2, 9, 0xdead, 0xdead, 1, 0, 0),
	CLKMON("f_fsspm_26m_ck", VLPCK, 1, 0x0270, 31, 1, 26000, 1),
	CLKMON("f_fulposc_sspm_ck", VLPCK, 2, 0x0270, 30, 1, 260000, 1),
	CLKMON("hf_fsspm_ck", VLPCK, 3, 0x0270, 29, 1, 364000, 1),
	CLKMON("hf_fspm_ck", VLPCK, 4, 0x0270, 28, 1, 130000, 1),
	CLKMON("hf_faxi_vlp_ck", VLPCK, 5, 0x0270, 27, 1, 156000, 1),
	CLKMON("hf_fnoc_vlp_ck", VLPCK, 6, 0x0270, 26, 1, 242667, 1),
	CLKMON("hf_fpwm_vlp_ck", VLPCK, 7, 0x0270, 25, 1, 65000, 1),
	CLKMON("hf_fsystimer_26m_ck", VLPCK, 8, 0x0270, 24, 1, 26000, 1),
	CLKMON("hf_fdpsw_ck", VLPCK, 9, 0x0270, 23, 1, 74286, 1),
	CLKMON("hf_fdpsw_central_ck", VLPCK, 10, 0x0270, 22, 1, 74286, 1),
	CLKMON("hf_fsrck_ck", VLPCK, 11, 0x0270, 21, 1, 26000, 1),
	CLKMON("hf_fdvfsrc_ck", VLPCK, 12, 0x0270, 20, 1, 26000, 1),
	CLKMON("hf_fkp_irq_gen_ck", VLPCK, 13, 0x0270, 19, 1, 156000, 1),
	CLKMON("hf_fdebug_err_flag_vlp_26m_ck", VLPCK, 14, 0x0270, 18, 1, 26000, 1),
	CLKMON("f_fips_ck", VLPCK, 15, 0x0270, 17, 1, 26000, 1),
	CLKMON("hf_fdpmsrdma_ck", VLPCK, 16, 0x0270, 16, 1, 156000, 1),
	CLKMON("f_fvlp_pbus_ck", VLPCK, 17, 0x0270, 15, 1, 312000, 1),
	CLKMON("f_fvlp_pbus_26m_ck", VLPCK, 18, 0x0270, 14, 1, 26000, 1),
	CLKMON("f_fvcore_pbus_ck", VLPCK, 19, 0x0270, 13, 1, 312000, 1),
	CLKMON("f_fvcore_pbus_26m_ck", VLPCK, 20, 0x0270, 12, 1, 26000, 1),
	CLKMON("f_fcamtg0_ck", VLPCK, 21, 0x0270, 11, 1, 24000, 1),
	CLKMON("f_fcamtg1_ck", VLPCK, 22, 0x0270, 10, 1, 24000, 1),
	CLKMON("f_fcamtg2_ck", VLPCK, 23, 0x0270, 9, 1, 26000, 1),
	CLKMON("f_fcamtg3_ck", VLPCK, 24, 0x0270, 8, 1, 24000, 1),
	CLKMON("f_fcamtg4_ck", VLPCK, 25, 0x0270, 7, 1, 24000, 1),
	CLKMON("f_fcamtg5_ck", VLPCK, 26, 0x0270, 6, 1, 24000, 1),
	CLKMON("f_fcamtg6_ck", VLPCK, 27, 0x0270, 5, 1, 24000, 1),
	CLKMON("f_fcamtg7_ck", VLPCK, 28, 0x0270, 4, 1, 24000, 1),
	CLKMON("hf_faud_engen1_ck", VLPCK, 29, 0x0270, 3, 1, 45158, 1),
	CLKMON("hf_faud_engen2_ck", VLPCK, 30, 0x0270, 2, 1, 49152, 1),
	CLKMON("hf_faud_sw_engen1_ck", VLPCK, 31, 0x0270, 1, 1, 22579, 1),
	CLKMON("hf_faud_sw_engen2_ck", VLPCK, 32, 0x0270, 0, 1, 24576, 1),
	CLKMON("hf_faud_intbus_ck", VLPCK, 33, 0x0274, 31, 1, 26000, 1),
	CLKMON("hf_faudio_h_ck", VLPCK, 34, 0x0274, 30, 1, 196608, 1),
	CLKMON("f_fusb_top_ck", VLPCK, 35, 0x0274, 29, 1, 260000, 1),
	CLKMON("f_fssusb_xhci_ck", VLPCK, 36, 0x0274, 28, 1, 260000, 1),
	CLKMON("f_fspu_vlp_26m_ck", VLPCK, 37, 0x0274, 27, 1, 26000, 1),
	CLKMON("hf_fspu0_vlp_ck", VLPCK, 38, 0x0274, 26, 1, 436800, 1),
	CLKMON("hf_fspu1_vlp_ck", VLPCK, 39, 0x0274, 25, 1, 436800, 1),
	CLKMON("hf_fcrywrapper_vlp_ck", VLPCK, 40, 0x0274, 24, 1, 273000, 1),
	CLKMON("hf_fscp_ck", VLPCK, 41, 0x0274, 23, 1, 26000, 1),
	CLKMON("hf_fscp_spi_ck", VLPCK, 42, 0x0274, 22, 1, 26000, 1),
	CLKMON("hf_fscp_iic_ck", VLPCK, 43, 0x0274, 21, 1, 26000, 1),
	CLKMON("hf_fscp_iic_high_spd_ck", VLPCK, 44, 0x0274, 20, 1, 26000, 1),
	CLKMON("hf_fscp_ois_ck", VLPCK, 45, 0x0274, 19, 1, 26000, 1),
	CLKMON("hf_fusb_mem_vlp_ck", VLPCK, 46, 0x0274, 18, 1, 173333, 1),
	CLKMON("f_fdisp_pwm_ck", VLPCK, 47, 0x0274, 17, 1, 130000, 1),
	CLKMON("hf_fcksys_rsv_ck", VLPCK, 48, 0x0274, 16, 1, 78000, 1),
	CLKMON("f_fpwrap_ulposc_ck", VLPCK, 49, 0x0274, 15, 1, 26000, 1),
	CLKMON("hf_ftia_ck", VLPCK, 50, 0x0274, 14, 1, 26000, 1),
	CLKMON("hf_fspmi_m_mst_ck", VLPCK, 51, 0x0274, 13, 1, 26000, 1),
	CLKMON("hf_fhvs_ck", VLPCK, 52, 0x0274, 12, 1, 26000, 1),
	PLL_MON("MAINPLL_TST_CK", SUBSYS, 0, 0, 2184000, MAINPLL_DB, 1),
	PLL_MON("UNIVPLL_TST_CK", SUBSYS, 0, 0, 2496000, UNIVPLL_DB, 1),
	PLL_MON("MSDCPLL_TST_CK", SUBSYS, 0, 0, 416000, MSDCPLL_DB, 1),
	PLL_MON("EMIPLL_TST_CK", SUBSYS, 0, 0, 2540000, EMIPLL_DB, 1),
	PLL_MON("MAINPLL2_TST_CK", SUBSYS, 0, 0, 2184000, MAINPLL2_DB, 1),
	PLL_MON("UNIVPLL2_TST_CK", SUBSYS, 0, 0, 2496000, UNIVPLL2_DB, 1),
	PLL_MON("MMPLL_TST_CK", SUBSYS, 0, 0, 2750000, MMPLL_DB, 1),
	PLL_MON("IMGPLL_TST_CK", SUBSYS, 0, 0, 2640000, IMGPLL_DB, 1),
	PLL_MON("TVDPLL_TST_CK", SUBSYS, 0, 0, 594000, TVDPLL_DB, 1),
	PLL_MON("APLL1_TST_CK", SUBSYS, 0, 0, 180634, APLL1_DB, 1),
	PLL_MON("APLL2_TST_CK", SUBSYS, 0, 0, 196608, APLL2_DB, 1),
	PLL_MON("CCIPLL_TST_CK", SUBSYS, 0, 0, 0, CCIPLL_DB, 0),
	PLL_MON("PTPPLL_TST_CK", SUBSYS, 0, 0, 0, PTPPLL_DB, 0),
	PLL_MON("CLKSQR",CLKSQ, 0, 0, 26000, CLKSQR_DB, 1),
	{},
};

const struct fmeter_clk *mt6993_get_fmeter_clks(void)
{
	return fclks_arr;
}

void __iomem *fqmtr_remap(enum DOMAIN_BASE domain, uint32_t ofs) {
	void __iomem *remap_addr = NULL;

	switch (domain) {
		case CKSYS_DB:
			remap_addr = (fm_base[FM_CKSYS_TOP] + ofs);
			break;
		case CKSYS2_DB:
			remap_addr = (fm_base[FM_CKSYS_MM] + ofs);
			break;
		case VLPCK_DB:
			remap_addr = (fm_base[FM_CKSYS_VLP] + ofs);
			break;
		case CKMTR_TOP_DB:
			remap_addr = (fm_base[FM_CKSYS_TOP_CKMTR] + ofs);
			break;
		case CKMTR_MM_DB:
			remap_addr = (fm_base[FM_CKSYS_MM_CKMTR] + ofs);
			break;
		case CKMTR_VLP_DB:
			remap_addr = (fm_base[FM_CKSYS_VLP_CKMTR]  + ofs);
			break;
		case CLKSQR_DB:
			remap_addr = (fm_base[FM_CLKSQR] + ofs);
			break;
		case MAINPLL_DB:
			remap_addr = (fm_base[FM_MAINPLL_CTRL] + ofs);
			break;
		case UNIVPLL_DB:
			remap_addr = (fm_base[FM_UNIVPLL_CTRL] + ofs);
			break;
		case MSDCPLL_DB:
			remap_addr = (fm_base[FM_MSDCPLL_CTRL] + ofs);
			break;
		case EMIPLL_DB:
			remap_addr = (fm_base[FM_EMIPLL_CTRL] + ofs);
			break;
		case MAINPLL2_DB:
			remap_addr = (fm_base[FM_MAINPLL2_CTRL] + ofs);
			break;
		case UNIVPLL2_DB:
			remap_addr = (fm_base[FM_UNIVPLL2_CTRL] + ofs);
			break;
		case MMPLL_DB:
			remap_addr = (fm_base[FM_MMPLL_CTRL] + ofs);
			break;
		case IMGPLL_DB:
			remap_addr = (fm_base[FM_IMGPLL_CTRL] + ofs);
			break;
		case TVDPLL_DB:
			remap_addr = (fm_base[FM_TVDPLL_CTRL] + ofs);
			break;
		case APLL1_DB:
			remap_addr = (fm_base[FM_APLL1_CTRL] + ofs);
			break;
		case APLL2_DB:
			remap_addr = (fm_base[FM_APLL2_CTRL] + ofs);
			break;
		case CCIPLL_DB:
			remap_addr = (fm_base[FM_CCIPLL] + ofs);
			break;
		case PTPPLL_DB:
			remap_addr = (fm_base[FM_PTPPLL] + ofs);
			break;
		default:
			remap_addr = 0;
			break;
	}

	if (!remap_addr) {
		fq_pr_err("%s: addr Null\n", __func__);
		configASSERT(0);
		return 0;
	}

	/* fqmtr address remap */
	return remap_addr;
}



uint32_t cm_read(enum DOMAIN_BASE domain, uint32_t ofs) {
	return (uint32_t)FQMTR_READL(fqmtr_remap(domain, ofs));
}

void cm_write(uint32_t val, enum DOMAIN_BASE domain, uint32_t ofs) {
	void __iomem *remap = fqmtr_remap(domain, ofs);

	if(!remap)
		return;

	FQMTR_WRITEL((uint32_t)val, remap);
	return;
}


// Function Definitions
uint32_t fqmtr_cal(enum DOMAIN_BASE domain, uint32_t cali_mode, uint32_t fqmtr_div, uint32_t load_cnt,
				   uint32_t fqmtr_clkmux_sel, uint32_t refck_clmux_sel, uint32_t eo) {
	uint32_t ckmtr_con0, ckmtr_con1, ckmtr_con2, ckmtr_con3;
	uint32_t cal_cnt;
	uint32_t freq;
	uint32_t fqmtr_cnt = 0;

	// Backup registers
	ckmtr_con0 = cm_read(domain, CLKMON_REG_CON0_OFS(eo));
	ckmtr_con1 = cm_read(domain, CLKMON_REG_CON1_OFS(eo));
	ckmtr_con2 = cm_read(domain, CLKMON_REG_CON2_OFS(eo));
	ckmtr_con3 = cm_read(domain, CLKMON_REG_CON3_OFS(eo));

	/* TINFO = "Step 1: Set HW MODE to 0" */
	cm_write(cm_read(domain, CLKMON_REG_CON3_OFS(eo)) & ~0x1U, domain, CLKMON_REG_CON3_OFS(eo));

	/* TINFO = "Step 2: Wait for FQMTR to finish" */
	while (cm_read(domain, CLKMON_REG_CON6_OFS(eo)) & 0x1) {
		fqmtr_cnt++;
		if (fqmtr_cnt > FQMTR_TIMEOUT_CNT) {
			fq_pr_err("Get DOMAIN_BASE %d FAIL\n", domain);
			return 0;
		}
	}

	/* TINFO = "Step 3: Clear specific bits in CLKMON_REG_CON0" */
	cm_write(cm_read(domain, CLKMON_REG_CON0_OFS(eo)) & ~(0x1U << 15) & ~(0x1U << 8) & ~(0x1U << 4), domain, CLKMON_REG_CON0_OFS(eo));

	/* TINFO = "Step 4: Set specific bits in CLKMON_REG_CON0" */
	cm_write(cm_read(domain, CLKMON_REG_CON0_OFS(eo)) | (0x1 << 15) | (0x1 << 12) | (cali_mode << 8), domain, CLKMON_REG_CON0_OFS(eo));

	/* TINFO = "Step 5: Clear load_cnt in CLKMON_REG_CON1" */
	cm_write(cm_read(domain, CLKMON_REG_CON1_OFS(eo)) & ~(0x3FFU << 16), domain, CLKMON_REG_CON1_OFS(eo));

	/* TINFO = "Step 6: Set load_cnt in CLKMON_REG_CON1" */
	cm_write(cm_read(domain, CLKMON_REG_CON1_OFS(eo)) | (load_cnt << 16), domain, CLKMON_REG_CON1_OFS(eo));

	/* TINFO = "Step 7: Clear specific bits in CLKMON_REG_CON2" */
	cm_write(cm_read(domain, CLKMON_REG_CON2_OFS(eo)) & ~(0xFFU << 24) & ~(0x1U << 4) & ~(0x1U << 3) & ~(0x1U << 2) & ~(0x3U << 0), domain, CLKMON_REG_CON2_OFS(eo));

	/* TINFO = "Step 8: Set specific bits in CLKMON_REG_CON2" */
	cm_write(cm_read(domain, CLKMON_REG_CON2_OFS(eo)) | (fqmtr_div << 24) | (refck_clmux_sel << 2) | (fqmtr_clkmux_sel << 0), domain, CLKMON_REG_CON2_OFS(eo));

	/* TINFO = "Step 9: Trigger meter" */
	cm_write(cm_read(domain, CLKMON_REG_CON0_OFS(eo)) | (0x1U << 4), domain, CLKMON_REG_CON0_OFS(eo));

	/* TINFO = "Step 10: Wait for FQMTR to finish" */
	while (cm_read(domain, + CLKMON_REG_CON6_OFS(eo)) & 0x1U);

	/* TINFO = "Step 11: Clear trigger" */
	cm_write(cm_read(domain, CLKMON_REG_CON0_OFS(eo)) & ~(0x1U << 4), domain, CLKMON_REG_CON0_OFS(eo));

	/* TINFO = "Step 12: Read cali mode" */
	cali_mode = (cm_read(domain, CLKMON_REG_CON0_OFS(eo)) & (0x1 << 8)) >> 8;

	/* TINFO = "Step 13: Read cal_cnt" */
	cal_cnt = cm_read(domain, CLKMON_REG_CON1_OFS(eo)) & 0xFFFF;

	/* TINFO = "Step 14: Read load_cnt" */
	load_cnt = (cm_read(domain, CLKMON_REG_CON1_OFS(eo)) & (0x3FFU << 16)) >> 16;

	/* TINFO = "Step 15: Read fqmtr_div" */
	fqmtr_div = (cm_read(domain, CLKMON_REG_CON2_OFS(eo)) & (0xFFU << 24)) >> 24;

	/* TINFO = "Step 16: Calculate frequency" */
	if (cali_mode) {
		freq = 26000 * (load_cnt + 1) * (fqmtr_div + 1) / cal_cnt;
	} else {
		freq = 26000 * cal_cnt * (fqmtr_div + 1) / (load_cnt + 1);
	}

	// Restore registers
	cm_write(ckmtr_con0, domain, CLKMON_REG_CON0_OFS(eo));
	cm_write(ckmtr_con1, domain, CLKMON_REG_CON1_OFS(eo));
	cm_write(ckmtr_con2, domain, CLKMON_REG_CON2_OFS(eo));
	cm_write(ckmtr_con3, domain, CLKMON_REG_CON3_OFS(eo));

	return freq;
}

uint32_t cksys_top_fqmtr(uint32_t arr_id) {
	uint32_t ID = fclks_arr[arr_id].id;
	uint32_t fenc_ofs = fclks_arr[arr_id].fenc_ofs;
	uint32_t fenc_bit = fclks_arr[arr_id].fenc_bit;

	uint32_t meter_clk_freq = 0;
	uint32_t fqmtr_cnt = 0;

	/* TINFO = "Step 1: Wait for fenc_status to be 0x1" */
	while ((cm_read(CKSYS_DB, fenc_ofs) & BIT(fenc_bit)) != BIT(fenc_bit)) {
		fqmtr_cnt++;
		if (fqmtr_cnt > FQMTR_TIMEOUT_CNT) {
			fq_pr_dbg("Get FQMTR ID %d fail, fenc_ofs: %x, fenc_addr_val: %x, fenc_bit:%x\n",
			ID, fenc_ofs, cm_read(CKSYS_DB, fenc_ofs),  fenc_bit);
			return 0;
		}
	}

	/* TINFO = "Step 2: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x7FU << 8), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 3: Set FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) | (ID << 8), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_TOP_DB , 0x0, 0x1, 0x3FF, 0x1, 0x0, 0x0);

	/* TINFO = "Step 5: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 6: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x7FU << 8), CKSYS_DB, clk_dbg_cfg_ofs);

	return meter_clk_freq;
}

uint32_t _cksys_top_abist_fqmtr(uint32_t ID) {
	uint32_t meter_clk_freq = 0;

	/* TINFO = "Step 1: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x3), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 2: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 3: Set ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) | (ID << 16 | 0x0), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Set fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) | (0x1 << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_TOP_DB, 0x0, 0x1, 0x3FF, 0x0, 0x0, 0x0);

	/* TINFO = "Step 6: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 7: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x1), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 8: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	return meter_clk_freq;
}

uint32_t _cksys_top_abist_fqmtr_pll(uint32_t ID, enum DOMAIN_BASE domain) {
	uint32_t posdiv, ckdiv, meter_clk_freq = 0;
	uint32_t rg_pll_ckdiv_en = (cm_read(domain, pll_con0_ofs) & BIT(17)) >> 17;

	/* TINFO = "Step 1: Read RG_PLL_POSDIV" */
	posdiv = (cm_read(domain, pll_con1_ofs) & (0x7 << 24)) >> 24;

	/* TINFO = "Step 2: Read RG_PLL_CKDIV" */
	ckdiv = (cm_read(domain, pll_con0_ofs) & (0xF << 9)) >> 9;

	if (!rg_pll_ckdiv_en) {
		/* TINFO = "Step 3: Enable CKDIV_CK for meter" */
		cm_write(cm_read(domain, pll_con0_ofs) | (0x1 << 17), domain, pll_con0_ofs);
	}

	/* TINFO = "Step 4: original _cksys_top_abist_fqmtr flow" */
	meter_clk_freq = _cksys_top_abist_fqmtr(ID);

	/* TINFO = "Step 5: fqmtr caculate with posdiv" */
	if (posdiv == 0) {
		meter_clk_freq = meter_clk_freq * ckdiv;
	} else if (posdiv == 1) {
		meter_clk_freq = meter_clk_freq * ckdiv / 2;
	} else if (posdiv == 2) {
		meter_clk_freq = meter_clk_freq * ckdiv / 4;
	} else if (posdiv == 3) {
		meter_clk_freq = meter_clk_freq * ckdiv / 8;
	} else if (posdiv == 4) {
		meter_clk_freq = meter_clk_freq * ckdiv / 16;
	} else {
		meter_clk_freq = meter_clk_freq * ckdiv / 1;
	}

	if (!rg_pll_ckdiv_en) {
		/* TINFO = "Step 6: Disable CKDIV_CK for meter" */
		cm_write(cm_read(domain, pll_con0_ofs) & ~(0x1U << 17), domain, pll_con0_ofs);
	}

	return meter_clk_freq;
}

uint32_t cksys_top_abist_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t id = 0;
	uint32_t is_from_pll = 0;

	if (arr_id < MAX_FQMTR_ARR_ID) {
		is_from_pll = fclks_arr[arr_id].is_pll;
		id = fclks_arr[arr_id].id;

		return is_from_pll ? _cksys_top_abist_fqmtr_pll(id, fclks_arr[arr_id].domain): _cksys_top_abist_fqmtr(id);
	} else
		fq_pr_err("[Error] arr_id: %d > MAX_FQMTR_ARR_ID\n", arr_id);
		return 0;
}

uint32_t _cksys_top_abist32k_fqmtr(uint32_t ID) {
	uint32_t meter_clk_freq = 0;

	/* TINFO = "Step 1: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x3), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 2: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 3: Set ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) | (ID << 16 | 0x0), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Set fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) | (0x1 << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_TOP_DB, 0x1, 0x0, 0xF, 0x0, 0x0, 0x0);

	/* TINFO = "Step 6: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 7: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x1), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 8: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	return meter_clk_freq;
}

uint32_t cksys_top_abist32k_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t id = 0;
	if (arr_id < MAX_FQMTR_ARR_ID) {
		id = fclks_arr[arr_id].id;
		return _cksys_top_abist32k_fqmtr(id);
	} else {
		fq_pr_err("[Error] arr_id: %d > MAX_FQMTR_ARR_ID\n", arr_id);
		return 0;
	}
}

uint32_t _cksys_top_abist2_fqmtr(uint32_t ID) {
	uint32_t meter_clk_freq = 0;

	/* TINFO = "Step 1: Clear ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 24 | 0x3), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 2: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 3: Set ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) | (ID << 24 | 0x2), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Set fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) | (0x1 << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_TOP_DB, 0x0, 0x1, 0x3FF, 0x2, 0x0, 0x0);

	/* TINFO = "Step 6: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 7: Clear ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS_DB, clk_dbg_cfg_ofs) & ~(0x1FU << 24 | 0x1), CKSYS_DB, clk_dbg_cfg_ofs);

	/* TINFO = "Step 8: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS_DB, clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS_DB, clk_misc_cfg_0_ofs);

	return meter_clk_freq;
}

uint32_t cksys_top_abist2_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t id = 0;
	if (arr_id < MAX_FQMTR_ARR_ID) {
		id = fclks_arr[arr_id].id;
		return _cksys_top_abist2_fqmtr(id);
	} else
		fq_pr_err("[Error] arr_id: %d > MAX_FQMTR_ARR_ID\n", arr_id);
		return 0;
}

uint32_t cksys_mm_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t meter_clk_freq = 0;
	uint32_t ID = fclks_arr[arr_id].id;
	uint32_t fenc_ofs = fclks_arr[arr_id].fenc_ofs;
	uint32_t fenc_bit = fclks_arr[arr_id].fenc_bit;
	uint32_t fqmtr_cnt = 0;

	/* TINFO = "Step 1: Wait for fenc_status to be 0x1" */
	while ((cm_read(CKSYS2_DB, fenc_ofs) & BIT(fenc_bit)) != BIT(fenc_bit)) {
		fqmtr_cnt++;
		if (fqmtr_cnt > FQMTR_TIMEOUT_CNT) {
			fq_pr_dbg("Get FQMTR ID %d fail, fenc_ofs: %x, fenc_addr_val: %x, fenc_bit:%x\n",
			ID, fenc_ofs, cm_read(CKSYS2_DB, fenc_ofs),  fenc_bit);
			return 0;
		}
	}

	/* TINFO = "Step 2: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x7FU << 8), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 3: Set FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) | (ID << 8), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_MM_DB, 0x0, 0x1, 0x3FF, 0x1, 0x0, 0x0);

	/* TINFO = "Step 5: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 6: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x7FU << 8), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	return meter_clk_freq;
}

uint32_t _cksys_mm_abist_fqmtr(uint32_t ID) {
	uint32_t meter_clk_freq = 0;

	/* TINFO = "Step 1: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x3), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 2: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	/* TINFO = "Step 3: Set ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) | (ID << 16 | 0x0), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Set fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) | (0x1 << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_MM_DB, 0x0, 0x1, 0x3FF, 0x0, 0x0, 0x0);

	/* TINFO = "Step 6: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 7: Clear ABIST_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x1FU << 16 | 0x1), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 8: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	return meter_clk_freq;
}

uint32_t _cksys_mm_abist_fqmtr_pll(uint32_t ID, enum DOMAIN_BASE domain) {
	uint32_t posdiv, ckdiv, meter_clk_freq = 0;
	uint32_t rg_pll_ckdiv_en = (cm_read(domain, pll_con0_ofs) & BIT(17)) >> 17;

	/* TINFO = "Step 1: Read RG_PLL_POSDIV" */
	posdiv = (cm_read(domain, pll_con1_ofs) & (0x7 << 24)) >> 24;

	/* TINFO = "Step 2: Read RG_PLL_CKDIV" */
	ckdiv = (cm_read(domain, pll_con0_ofs) & (0xF << 9)) >> 9;

	if (!rg_pll_ckdiv_en) {
		/* TINFO = "Step 3: Enable CKDIV_CK for meter" */
		cm_write(cm_read(domain, pll_con0_ofs) | (0x1 << 17), domain, pll_con0_ofs);
	}

	/* TINFO = "Step 4: original _cksys_top_abist_fqmtr flow" */
	meter_clk_freq = _cksys_mm_abist_fqmtr(ID);

	/* TINFO = "Step 5: fqmtr caculate with posdiv" */
	if (posdiv == 0) {
		meter_clk_freq = meter_clk_freq * ckdiv;
	} else if (posdiv == 1) {
		meter_clk_freq = meter_clk_freq * ckdiv / 2;
	} else if (posdiv == 2) {
		meter_clk_freq = meter_clk_freq * ckdiv / 4;
	} else if (posdiv == 3) {
		meter_clk_freq = meter_clk_freq * ckdiv / 8;
	} else if (posdiv == 4) {
		meter_clk_freq = meter_clk_freq * ckdiv / 16;
	} else {
		meter_clk_freq = meter_clk_freq * ckdiv / 1;
	}

	if (!rg_pll_ckdiv_en) {
		/* TINFO = "Step 6: Disable CKDIV_CK for meter" */
		cm_write(cm_read(domain, pll_con0_ofs) & ~(0x1U << 17), domain, pll_con0_ofs);
	}

	return meter_clk_freq;
}

uint32_t cksys_mm_abist_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t id = 0;
	uint32_t is_from_pll = 0;

	if (arr_id < MAX_FQMTR_ARR_ID) {

		is_from_pll = fclks_arr[arr_id].is_pll;
		id = fclks_arr[arr_id].id;
		return is_from_pll ? _cksys_mm_abist_fqmtr_pll(id, fclks_arr[arr_id].domain): _cksys_mm_abist_fqmtr(id);
	} else {
		fq_pr_err("[Error] arr_id: %d > MAX_FQMTR_ARR_ID\n", arr_id);
		return 0;
	}
}

uint32_t _cksys_mm_abist2_fqmtr(uint32_t ID) {
	uint32_t meter_clk_freq = 0;

	/* TINFO = "Step 1: Clear ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x1FU << 24 | 0x3), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 2: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	/* TINFO = "Step 3: Set ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) | (ID << 24 | 0x2), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 4: Set fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) | (0x1 << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_MM_DB, 0x0, 0x1, 0x3FF, 0x2, 0x0, 0x0);

	/* TINFO = "Step 6: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 7: Clear ABIST2_DBGMUX SEL" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_dbg_cfg_ofs) & ~(0x1FU << 24 | 0x1), CKSYS2_DB, cksys2_clk_dbg_cfg_ofs);

	/* TINFO = "Step 8: Clear fqmtr divider" */
	cm_write(cm_read(CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs) & ~(0xFFU << 24), CKSYS2_DB, cksys2_clk_misc_cfg_0_ofs);

	return meter_clk_freq;
}

uint32_t cksys_mm_abist2_fqmtr(enum FQMTR_ARR_ID arr_id) {
	uint32_t id = 0;
	if (arr_id < MAX_FQMTR_ARR_ID) {
		id = fclks_arr[arr_id].id;
		return _cksys_mm_abist2_fqmtr(id);
	} else {
		fq_pr_err("[Error] arr_id: %d > MAX_FQMTR_ARR_ID\n", arr_id);
		return 0;
	}
}

uint32_t cksys_vlp_fqmtr(uint32_t arr_id) {
	uint32_t meter_clk_freq = 0;
	uint32_t ID = fclks_arr[arr_id].id;
	uint32_t fenc_ofs = fclks_arr[arr_id].fenc_ofs;
	uint32_t fenc_bit = fclks_arr[arr_id].fenc_bit;
	uint32_t fqmtr_cnt = 0;

	/* TINFO = "Step 1: Wait for fenc_status to be 0x1" */
	while ((cm_read(VLPCK_DB, fenc_ofs) & BIT(fenc_bit)) != BIT(fenc_bit)) {
		fqmtr_cnt++;
		if (fqmtr_cnt > FQMTR_TIMEOUT_CNT) {
			fq_pr_dbg("Get FQMTR ID %d fail, fenc_ofs: %x, fenc_addr_val: %x, fenc_bit:%x\n",
			ID, fenc_ofs, cm_read(VLPCK_DB, fenc_ofs),  fenc_bit);
			return 0;
		}
	}

	/* TINFO = "Step 2: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(VLPCK_DB, vlp_fqmtr_con0_ofs) & ~(0x7FU << 16), VLPCK_DB, vlp_fqmtr_con0_ofs);

	/* TINFO = "Step 3: Set FQMTR_DBGMUX SEL" */
	cm_write(cm_read(VLPCK_DB, vlp_fqmtr_con0_ofs) | (ID << 16), VLPCK_DB, vlp_fqmtr_con0_ofs);

	/* TINFO = "Step 4: Call FQMTR function" */
	meter_clk_freq = fqmtr_cal(CKMTR_VLP_DB, 0x0, 0x1, 0x3FF, 0x0, 0x0, 0x0);

	/* TINFO = "Step 5: fq_pr_err the meter frequency" */
	//fq_pr_err("meter frequency = %d KHz\n", meter_clk_freq);

	/* TINFO = "Step 6: Clear FQMTR_DBGMUX SEL" */
	cm_write(cm_read(VLPCK_DB, vlp_fqmtr_con0_ofs) & ~(0x7FU << 16), VLPCK_DB, vlp_fqmtr_con0_ofs);

	return meter_clk_freq;
}

uint32_t _pll_fqmtr(enum DOMAIN_BASE domain) {
	uint32_t posdiv, ckdiv, fqmtr_freq = 0;

	/* TINFO = "Step 1: Read RG_PLL_POSDIV" */
	posdiv = (cm_read(domain, pll_con1_ofs) & (0x7 << 24)) >> 24;

	/* TINFO = "Step 2: Read RG_PLL_CKDIV" */
	ckdiv = (cm_read(domain, pll_con0_ofs) & (0xF << 9)) >> 9;

	/* TINFO = "Step 3: Read RG_PLL_SDM_PCW" */
	//sdm_pcw = cm_read(PLL_CTRL_BASE_ADDR + pll_con1_ofs) & 0x3FFFFF;
	//fq_pr_err("PLL domain %d, sdm_pcw = %x\n", domain, sdm_pcw);

	/* TINFO = "Step 4: Enable AD_PLL_TST_CK for meter" */
	cm_write(cm_read(domain, pll_con0_ofs) | (0x1 << 17) | (0x1 << 15), domain, pll_con0_ofs);

	/* TINFO = "Step 5: Call FQMTR function" */
	fqmtr_freq = fqmtr_cal(domain, 0x0, 0x0, 0x3FF, 0x0, 0x0, ckmtr_base_ofs);

	/* TINFO = "Step 6: Calculate PLL frequency (KHz)" */
	if (posdiv == 0) {
		fqmtr_freq = fqmtr_freq * ckdiv;
	} else if (posdiv == 1) {
		fqmtr_freq = fqmtr_freq * ckdiv / 2;
	} else if (posdiv == 2) {
		fqmtr_freq = fqmtr_freq * ckdiv / 4;
	} else if (posdiv == 3) {
		fqmtr_freq = fqmtr_freq * ckdiv / 8;
	} else if (posdiv == 4) {
		fqmtr_freq = fqmtr_freq * ckdiv / 16;
	} else {
		fqmtr_freq = fqmtr_freq * ckdiv / 1;
	}

	// fq_pr_err the calculated PLL frequency
	//fq_pr_err("PLL frequency = %d KHz\n", fqmtr_freq);

	return fqmtr_freq;
}

uint32_t pll_fqmtr(enum FQMTR_ARR_ID arr_id) {
	if (fclks_arr[arr_id].type == SUBSYS) {
		return _pll_fqmtr(fclks_arr[arr_id].domain);
	} else {
		fq_pr_err("[Error] pll_fqmtr: %d != SUBSYS\n", arr_id);
		return 0;
	}
}

uint32_t clksq_fqmtr(void) {
	uint32_t ckmtr_con0, ckmtr_con1, ckmtr_con2, ckmtr_con3;
	uint32_t cal_cnt, load_cnt, fqmtr_div;
	uint32_t freq;
	uint32_t fqmtr_cnt = 0;

	/* TINFO = "Step 1-4: Backup registers" */
	ckmtr_con0 = cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));
	ckmtr_con1 = cm_read(CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs));
	ckmtr_con2 = cm_read(CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs));
	ckmtr_con3 = cm_read(CLKSQR_DB, CLKMON_REG_CON3_OFS(clkmon_con_ofs));

	/* TINFO = "Step 5: Set HW MODE to 0" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON3_OFS(clkmon_con_ofs)) & ~0x1U, CLKSQR_DB, CLKMON_REG_CON3_OFS(clkmon_con_ofs));

	/* TINFO = "Step 6: Wait for FQMTR to finish" */
	while (cm_read(CLKSQR_DB, CLKMON_REG_CON6_OFS(clkmon_con_ofs)) & 0x1) {
		fqmtr_cnt++;
		if (fqmtr_cnt > FQMTR_TIMEOUT_CNT) {
			fq_pr_err("Get CLKSQR_DB FAIL\n");
			return 0;
		}
	}

	/* TINFO = "Step 7: Clear specific bits in CLKMON_REG_CON0" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs)) & ~(0x1U << 15) & ~(0x1U << 8) & ~(0x1U << 4), CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));

	/* TINFO = "Step 8: Set specific bits in CLKMON_REG_CON0" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs)) | (0x1 << 15) | (0x1 << 12) | (0x0 << 8), CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));

	/* TINFO = "Step 9: Clear load_cnt in CLKMON_REG_CON1" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs)) & ~(0x3FFU << 16), CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs));

	/* TINFO = "Step 10: Set load_cnt in CLKMON_REG_CON1" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs)) | (0x3U << 16), CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs));

	/* TINFO = "Step 11: Clear specific bits in CLKMON_REG_CON2" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs)) & ~(0xFFU << 24) & ~(0x1U << 4) & ~(0x1U << 3) & ~(0x1U << 2) & ~(0x3U << 0), CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs));

	/* TINFO = "Step 12: Set specific bits in CLKMON_REG_CON2" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs)) | (0x0 << 24) | (0x0 << 2) | (0x0 << 0), CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs));

	/* TINFO = "Step 13: Trigger meter" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs)) | (0x1 << 4), CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));

	/* TINFO = "Step 14: Wait for FQMTR to start" */
	while (!(cm_read(CLKSQR_DB, CLKMON_REG_CON6_OFS(clkmon_con_ofs)) & 0x1));

	/* TINFO = "Step 15: Wait for FQMTR to finish" */
	while (cm_read(CLKSQR_DB, CLKMON_REG_CON6_OFS(clkmon_con_ofs)) & 0x1);

	/* TINFO = "Step 16: Clear trigger" */
	cm_write(cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs)) & ~(0x1U << 4), CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));

	/* TINFO = "Step 17: Read cali mode" */
	//cali_mode = (cm_read(CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs)) & (0x1 << 8)) >> 8;
	//fq_pr_err("PLL domain %d, cali_mode = %x\n", domain, cali_mode);

	/* TINFO = "Step 18: Read cal_cnt" */
	cal_cnt = cm_read(CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs)) & 0xFFFFU;

	/* TINFO = "Step 19: Read load_cnt" */
	load_cnt = (cm_read(CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs)) & (0x3FFU << 16)) >> 16;

	/* TINFO = "Step 20: Read fqmtr_div" */
	fqmtr_div = (cm_read(CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs)) & (0xFFU << 24)) >> 24;

	/* TINFO = "Step 21: Calculate frequency" */
	freq = 32 * cal_cnt * (fqmtr_div + 1) / (load_cnt + 1);

	/* TINFO = "Step 22-25: Restore registers" */
	cm_write(ckmtr_con0, CLKSQR_DB, CLKMON_REG_CON0_OFS(clkmon_con_ofs));
	cm_write(ckmtr_con1, CLKSQR_DB, CLKMON_REG_CON1_OFS(clkmon_con_ofs));
	cm_write(ckmtr_con2, CLKSQR_DB, CLKMON_REG_CON2_OFS(clkmon_con_ofs));
	cm_write(ckmtr_con3, CLKSQR_DB, CLKMON_REG_CON3_OFS(clkmon_con_ofs));

	return freq;
}

static unsigned int mt6993_get_fmeter_freq(unsigned int arr_id,
		enum FMETER_TYPE type)
{
	uint32_t i = arr_id;
	uint32_t cur_freq = 0;
	unsigned long flags;

	fmeter_lock(flags);
	switch (fclks_arr[i].type) {
		case (ABIST):
			cur_freq = cksys_top_abist_fqmtr(i);
			break;
		case (ABIST_2):
			cur_freq = cksys_top_abist2_fqmtr(i);
			break;
		case (CKGEN):
			/* input should be array index to get fenc info*/
			cur_freq = cksys_top_fqmtr(i);
			break;
		case (ABIST_CK2):
			cur_freq = cksys_mm_abist_fqmtr(i);
			break;
		case (ABIST_2_CK2):
			cur_freq = cksys_mm_abist2_fqmtr(i);
			break;
		case (CKGEN_CK2):
			/* input should be array index to get fenc info*/
			cur_freq = cksys_mm_fqmtr(i);
			break;
		case (VLPCK):
			/* input should be array index to get fenc info*/
			cur_freq = cksys_vlp_fqmtr(i);
			break;
		case (CLKSQ):
			cur_freq = clksq_fqmtr();
			break;
		case (SUBSYS):
			cur_freq = pll_fqmtr(i);
			break;
		case (ABIST32K):
			cur_freq = cksys_top_abist32k_fqmtr(i);
			break;
		default:
			cur_freq = 0;
			fq_pr_err("unknow type\n");
			return FT_NULL;
	}
	fmeter_unlock(flags);

	return cur_freq;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {

		base = of_iomap(node, 0);

		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6993_get_fmeter_clks,
	.get_fmeter_freq = mt6993_get_fmeter_freq,
};

static int clk_fmeter_mt6993_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < FM_SYS_NUM; i++) {
		fm_base[i] = get_base_from_comp(comp_list[i]);
		if (IS_ERR(fm_base[i]))
			goto ERR;

	}

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s(%s) can't find base\n", __func__, comp_list[i]);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6993_drv = {
	.probe = clk_fmeter_mt6993_probe,
	.driver = {
		.name = "clk-fmeter-mt6993",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6993", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6993_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6993_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
