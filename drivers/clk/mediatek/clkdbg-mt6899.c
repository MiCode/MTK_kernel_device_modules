// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"

const char * const *get_mt6899_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_faxi_sel",
		"ufs_faxi_sel",
		"pextp_faxi_sel",
		"bus_aximem_sel",
		"mem_sub_sel",
		"peri_fmem_sub_sel",
		"ufs_fmem_sub_sel",
		"pextp_fmem_sub_sel",
		"emi_n_sel",
		"emi_s_sel",
		"emi_slice_n_sel",
		"emi_slice_s_sel",
		"ap2conn_host_sel",
		"atb_sel",
		"cirq_sel",
		"efuse_sel",
		"mcu_l3gic_sel",
		"mcu_infra_sel",
		"mcu_acp_sel",
		"tl_sel",
		"md_emi_sel",
		"dsp_sel",
		"mfg_ref_sel",
		"mfgsc_ref_sel",
		"mfg_eb_sel",
		"uart_sel",
		"spi0_b_sel",
		"spi1_b_sel",
		"spi2_b_sel",
		"spi3_b_sel",
		"spi4_b_sel",
		"spi5_b_sel",
		"spi6_b_sel",
		"spi7_b_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"aud_intbus_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"msdc30_1_h_sel",
		"i2c_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"ufs_mbist_sel",
		"pextp_mbist_sel",
		"aud_1_sel",
		"aud_2_sel",
		"audio_h_sel",
		"adsp_sel",
		"adps_uarthub_b_sel",
		"dpmaif_main_sel",
		"pwm_sel",
		"mcupm_sel",
		"dpsw_cmp_26m_sel",
		"msdc30_2_h_sel",
		"apll_i2sin0_m_sel",
		"apll_i2sin1_m_sel",
		"apll_i2sin2_m_sel",
		"apll_i2sin3_m_sel",
		"apll_i2sin4_m_sel",
		"apll_i2sin6_m_sel",
		"apll_i2sout0_m_sel",
		"apll_i2sout1_m_sel",
		"apll_i2sout2_m_sel",
		"apll_i2sout3_m_sel",
		"apll_i2sout4_m_sel",
		"apll_i2sout6_m_sel",
		"apll_fmi2s_m_sel",
		"apll_tdmout_m_sel",
		"seninf0_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"seninf4_sel",
		"seninf5_sel",
		"ccu_ahb_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"camtm_sel",
		"dpe_sel",
		"vdec_sel",
		"ccusys_sel",
		"ccutm_sel",
		"venc_sel",
		"dp_core_sel",
		"dp_sel",
		"disp_sel",
		"mdp_sel",
		"mminfra_sel",
		"mmup_sel",
		"img_26m_sel",
		"cam_26m_sel",
		"dsi_occ_sel",

		/* topckgen */
		"apll12_div_i2sin0",
		"apll12_div_i2sin1",
		"apll12_div_i2sin2",
		"apll12_div_i2sin3",
		"apll12_div_i2sin4",
		"apll12_div_i2sin6",
		"apll12_div_i2sout0",
		"apll12_div_i2sout1",
		"apll12_div_i2sout2",
		"apll12_div_i2sout3",
		"apll12_div_i2sout4",
		"apll12_div_i2sout6",
		"apll12_div_fmi2s",
		"apll12_div_tdmout_m",
		"apll12_div_tdmout_b",

		/* infra_infracfg_ao_reg */
		"infracfg_ao_ccif1_ap",
		"infracfg_ao_ccif1_md",
		"infracfg_ao_ccif_ap",
		"infracfg_ao_ccif_md",
		"infracfg_ao_cldmabclk",
		"infracfg_ao_ccif5_md",
		"infracfg_ao_ccif2_ap",
		"infracfg_ao_ccif2_md",
		"infracfg_ao_dpmaif_main",
		"infracfg_ao_ccif4_md",
		"infracfg_ao_dpmaif_26m",

		/* apmixedsys */
		"mainpll",
		"univpll",
		"mmpll",
		"emipll",
		"apll1",
		"apll2",
		"msdcpll",
		"emipll2",
		"imgpll",
		"tvdpll",
		"adsppll",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
		"peraop_uart2",
		"peraop_uart3",
		"peraop_pwm_h",
		"peraop_pwm_b",
		"peraop_pwm_fb1",
		"peraop_pwm_fb2",
		"peraop_pwm_fb3",
		"peraop_pwm_fb4",
		"peraop_spi0_b",
		"peraop_spi1_b",
		"peraop_spi2_b",
		"peraop_spi3_b",
		"peraop_spi4_b",
		"peraop_spi5_b",
		"peraop_spi6_b",
		"peraop_spi7_b",
		"peraop_dma_b",
		"peraop_ssusb0_frmcnt",
		"peraop_msdc1",
		"peraop_msdc1_f",
		"peraop_msdc1_h",
		"peraop_msdc2",
		"peraop_msdc2_f",
		"peraop_msdc2_h",
		"peraop_audio_slv",
		"peraop_audio_mst",
		"peraop_audio_intbus",

		/* afe */
		"afe_dl1_dac_tml",
		"afe_dl1_dac_hires",
		"afe_dl1_dac",
		"afe_dl1_predis",
		"afe_dl1_nle",
		"afe_dl0_dac_tml",
		"afe_dl0_dac_hires",
		"afe_dl0_dac",
		"afe_dl0_predis",
		"afe_dl0_nle",
		"afe_pcm1",
		"afe_pcm0",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_fm_i2s",
		"afe_mtkaifv4",
		"afe_dmic1_aht",
		"afe_dmic1_adc_hires",
		"afe_dmic1_tml",
		"afe_dmic1_adc",
		"afe_dmic0_aht",
		"afe_dmic0_adc_hires",
		"afe_dmic0_tml",
		"afe_dmic0_adc",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in6",
		"afe_etdm_in4",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out4",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
		"afe_tdm_out",
		"afe_general3_asrc",
		"afe_general2_asrc",
		"afe_general1_asrc",
		"afe_general0_asrc",
		"afe_connsys_i2s_asrc",
		"afe_audio_hopping_ck",
		"afe_audio_f26m_ck",
		"afe_apll1_ck",
		"afe_apll2_ck",
		"afe_h208m_ck",
		"afe_apll_tuner2",
		"afe_apll_tuner1",

		/* imp_iic_wrap_c */
		"impc_i2c10",
		"impc_i2c11",
		"impc_i2c12",
		"impc_i2c13",

		/* ufscfg_ao */
		"ufsao_unipro_sys",
		"ufsao_u_phy_sap",
		"ufsao_u_phy_ahb_s_busck",
		"ufsao_u_ao_26m_ck",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",
		"ufspdn_ufshci_u_ahb",
		"ufspdn_ufshci_u_axi",
		"ufspdn_u_26m_ck",

		/* imp_iic_wrap_en */
		"impen_i3c2",

		/* imp_iic_wrap_e */
		"impe_i3c4",
		"impe_i3c8",

		/* imp_iic_wrap_s */
		"imps_i3c0",
		"imps_i3c1",
		"imps_i3c7",

		/* imp_iic_wrap_es */
		"impes_i3c9",

		/* imp_iic_wrap_w */
		"impw_i2c6",

		/* imp_iic_wrap_n */
		"impn_i2c3",
		"impn_i2c5",

		/* mfgpll_pll_ctrl */
		"mfgpll",

		/* mfgscpll_pll_ctrl */
		"mfgscpll",

		/* dispsys_config */
		"mm_config",
		"mm_disp_mutex0",
		"mm_disp_aal0",
		"mm_disp_aal1",
		"mm_disp_c3d0",
		"mm_disp_c3d1",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_ccorr2",
		"mm_disp_ccorr3",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_dither0",
		"mm_disp_dither1",
		"mm_disp_dither2",
		"mm_dli_async0",
		"mm_dli_async1",
		"mm_dli_async2",
		"mm_dli_async3",
		"mm_dli_async4",
		"mm_dli_async5",
		"mm_dli_async6",
		"mm_dli_async7",
		"mm_dlo_async0",
		"mm_dlo_async1",
		"mm_dlo_async2",
		"mm_dlo_async3",
		"mm_dlo_async4",
		"mm_disp_gamma0",
		"mm_disp_gamma1",
		"mm_mdp_aal0",
		"mm_mdp_rdma0",
		"mm_disp_oddmr0",
		"mm_disp_postalign0",
		"mm_disp_postmask0",
		"mm_disp_postmask1",
		"mm_disp_rsz0",
		"mm_disp_rsz1",
		"mm_disp_spr0",
		"mm_disp_tdshp0",
		"mm_disp_tdshp1",
		"mm_disp_wdma1",
		"mm_disp_y2r0",
		"mm_mdp_aal1",
		"mm_ssc",
		"mm_disp_rsz0_mout_relay",
		"mm_disp_rsz1_mout_relay",

		/* dispsys1_config */
		"mm1_dispsys1_config",
		"mm1_disp_mutex0",
		"mm1_disp_dli_async0",
		"mm1_disp_dli_async1",
		"mm1_disp_dli_async2",
		"mm1_mdp_rdma0",
		"mm1_disp_r2y0",
		"mm1_disp_splitter0",
		"mm1_disp_splitter1",
		"mm1_disp_vdcm0",
		"mm1_disp_dsc_wrap0",
		"mm1_disp_dsc_wrap1",
		"mm1_disp_dsc_wrap2",
		"mm1_DP_CLK",
		"mm1_CLK0",
		"mm1_CLK1",
		"mm1_CLK2",
		"mm1_disp_merge0",
		"mm1_disp_wdma0",
		"mm1_ssc",
		"mm1_disp_wdma1",
		"mm1_disp_wdma2",
		"mm1_disp_gdma0",
		"mm1_disp_dli_async3",
		"mm1_disp_dli_async4",
		"mm1_mod1",
		"mm1_mod2",
		"mm1_mod3",
		"mm1_mod4",
		"mm1_mod5",
		"mm1_mod6",
		"mm1_mod7",
		"mm1_subsys_ck",
		"mm1_dsi0_ck",
		"mm1_dsi1_ck",
		"mm1_dsi2_ck",
		"mm1_dp_ck",
		"mm1_f26m_ck",

		/* ovlsys_config */
		"ovlsys_config",
		"ovl_disp_fake_eng0",
		"ovl_disp_fake_eng1",
		"ovl_disp_mutex0",
		"ovl_disp_ovl0_2l",
		"ovl_disp_ovl1_2l",
		"ovl_disp_ovl2_2l",
		"ovl_disp_ovl3_2l",
		"ovl_disp_rsz1",
		"ovl_mdp_rsz0",
		"ovl_disp_wdma0",
		"ovl_disp_ufbc_wdma0",
		"ovl_disp_wdma2",
		"ovl_disp_dli_async0",
		"ovl_disp_dli_async1",
		"ovl_disp_dli_async2",
		"ovl_disp_dl0_async0",
		"ovl_disp_dl0_async1",
		"ovl_disp_dl0_async2",
		"ovl_disp_dl0_async3",
		"ovl_disp_dl0_async4",
		"ovl_disp_dl0_async5",
		"ovl_disp_dl0_async6",
		"ovl_inlinerot0",
		"ovl_ssc",
		"ovl_disp_y2r0",
		"ovl_disp_y2r1",
		"ovl_disp_ovl4_2l",

		/* imgsys_main */
		"img_fdvt",
		"img_me",
		"img_mmg",
		"img_larb12",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_adl_larb",
		"img_adlrd",
		"img_adlwr0",
		"img_avs",
		"img_ips",
		"img_adlwr1",
		"img_rootcq",
		"img_bls",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common2",
		"img_sub_common3",
		"img_sub_common4",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_wpe2",
		"img_gals_trx_ipe0",
		"img_gals_trx_ipe1",
		"img26",
		"img_bwr",
		"img_gals",

		/* dip_top_dip1 */
		"dip_dip1_dip_top",
		"dip_dip1_dip_gals0",
		"dip_dip1_dip_gals1",
		"dip_dip1_dip_gals2",
		"dip_dip1_dip_gals3",
		"dip_dip1_larb10",
		"dip_dip1_larb15",
		"dip_dip1_larb38",
		"dip_dip1_larb39",

		/* dip_nr1_dip1 */
		"dip_nr1_dip1_larb",
		"dip_nr1_dip1_dip_nr1",

		/* dip_nr2_dip1 */
		"dip_nr2_dip1_dip_nr",
		"dip_nr2_dip1_larb15",
		"dip_nr2_dip1_larb39",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",
		"wpe1_dip1_gals0",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",
		"wpe2_dip1_gals0",

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",
		"wpe3_dip1_gals0",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",
		"traw_dip1_gals",

		/* traw_cap_dip1 */
		"traw__dip1_cap",

		/* img_vcore_d1a */
		"img_vcore_gals_disp",
		"img_vcore_main",
		"img_vcore_sub0",
		"img_vcore_sub1",
		"img_vcore_img_26m",

		/* vdec_soc_gcon_base */
		"vde1_larb1_cken",
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_larb",
		"ven1_venc",
		"ven1_jpgenc",
		"ven1_jpgdec",
		"ven1_jpgdec_c1",
		"ven1_gals",
		"ven1_gals_sram",

		/* venc_gcon_core1 */
		"ven2_larb",
		"ven2_venc",
		"ven2_jpgenc",
		"ven2_jpgdec",
		"ven2_gals",
		"ven2_gals_sram",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_m_tia_sel",
		"vlp_apxgpt_26m_b_sel",
		"vlp_dpsw_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_srck_sel",
		"vlp_sramrc_sel",
		"vlp_spmi_p_sel",
		"vlp_ips_sel",
		"vlp_sspm_26m_sel",
		"vlp_ulposc_sspm_sel",
		"vlp_camtg0_sel",
		"vlp_camtg1_sel",
		"vlp_camtg2_sel",
		"vlp_camtg3_sel",
		"vlp_camtg4_sel",
		"vlp_camtg5_sel",
		"vlp_camtg6_sel",
		"vlp_camtg7_sel",
		"vlp_dpsw_central_sel",
		"vlp_toprgu_26m_sel",

		/* cam_main_r1a */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_larb27",
		"cam_m_larb29",
		"cam_m_cam",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_subc",
		"cam_m_cam_mraw",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv",
		"cam_m_adlrd",
		"cam_m_adlwr",
		"cam_m_fake_eng",
		"cam_m_cam2mm0_GCON_0",
		"cam_m_cam2mm1_GCON_0",
		"cam_m_cam2sys_GCON_0",
		"cam_m_cam2mm2_GCON_0",
		"cam_m_ips",
		"cam_m_cam_dpe",
		"cam_m_cam_asg",
		"cam_m_camsv_a_con_1",
		"cam_m_camsv_b_con_1",
		"cam_m_camsv_c_con_1",
		"cam_m_camsv_d_con_1",
		"cam_m_camsv_e_con_1",
		"cam_m_cam_qof_con_1",
		"cam_m_cam_bls_full_con_1",
		"cam_m_cam_bls_part_con_1",
		"cam_m_cam_bwr_con_1",
		"cam_m_cam_rtcq_con_1",
		"cam_m_cam2mm0_sub_c_dis",
		"cam_m_cam2mm1_sub_c_dis",
		"cam_m_cam2sys_sub_c_dis",
		"cam_m_cam2mm2_sub_c_dis",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_gals",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* camsys_ipe */
		"camsys_ipe_larb19",
		"camsys_ipe_dpe",
		"camsys_ipe_fus",
		"camsys_ipe_gals",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		"cam_ra_raw2mm_gals",
		"cam_ra_yuv2raw2mm",

		/* camsys_rmsa */
		"camsys_rmsa_larbx",
		"camsys_rmsa_cam",
		"camsys_rmsa_camtg",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		"cam_rb_raw2mm_gals",
		"cam_rb_yuv2raw2mm",

		/* camsys_rmsb */
		"camsys_rmsb_larbx",
		"camsys_rmsb_cam",
		"camsys_rmsb_camtg",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",
		"cam_rc_raw2mm_gals",
		"cam_rc_yuv2raw2mm",

		/* camsys_rmsc */
		"camsys_rmsc_larbx",
		"camsys_rmsc_cam",
		"camsys_rmsc_camtg",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",

		/* ccu_main */
		"ccu_larb19_con",
		"ccu2infra_GCON",
		"ccusys_ccu0_con",
		"ccu2mm0_GCON",

		/* cam_vcore_r1a */
		"camv_cv_camvcore",
		"camv_cv_cam_26m",
		"camv_cv_cam2mm0_subc_dis",
		"camv_cv_mm0_subc_dis",

		/* mminfra_ao_config */
		"mminfra_ao_gce_d",
		"mminfra_ao_gce_m",
		"mminfra_ao_gce_26m",

		/* mdpsys_config */
		"mdp_mutex0",
		"mdp_apb_bus",
		"mdp_smi0",
		"mdp_rdma0",
		"mdp_rdma2",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_color0",
		"mdp_wrot0",
		"mdp_fake_eng0",
		"mdp_apb_db",
		"mdp_birsz0",
		"mdp_c3d0",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_apb_bus",
		"mdp1_smi0",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_rdma2",
		"mdp1_mdp_hdr0",
		"mdp1_mdp_aal0",
		"mdp1_mdp_rsz0",
		"mdp1_mdp_tdshp0",
		"mdp1_mdp_color0",
		"mdp1_mdp_wrot0",
		"mdp1_mdp_fake_eng0",
		"mdp1_mdp_dli_async0",
		"mdp1_apb_db",
		"mdp1_mdp_rsz2",
		"mdp1_mdp_wrot2",
		"mdp1_mdp_dlo_async0",
		"mdp1_mdp_birsz0",
		"mdp1_mdp_c3d0",
		"mdp1_mdp_fg0",

		/* ccipll_pll_ctrl */
		"ccipll",

		/* armpll_ll_pll_ctrl */
		"armpll-ll",

		/* armpll_bl_pll_ctrl */
		"armpll-bl",

		/* armpll_b_pll_ctrl */
		"armpll-b",

		/* ptppll_pll_ctrl */
		"ptppll",
		NULL

	};

	return clks;
}


/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6899_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6899_all_clk_names,
};

static int clk_dbg_mt6899_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6899_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6899_drv = {
	.probe = clk_dbg_mt6899_probe,
	.driver = {
		.name = "clk-dbg-mt6899",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6899_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6899_drv, "clk-dbg-mt6899");
}

static void __exit clkdbg_mt6899_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6899_drv);
}

subsys_initcall(clkdbg_mt6899_init);
module_exit(clkdbg_mt6899_exit);
MODULE_LICENSE("GPL");
