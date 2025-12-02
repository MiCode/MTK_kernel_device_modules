// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/random.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clkchk-mt6993.h"
#include "clk-fmeter.h"

#define THREAD_LEN      (16)
#define THREAD_NUM      (6)
#define PD_NUM          (3)

static int clkdbg_thread_cnt;

const char * const *get_mt6993_all_clk_names(void)
{
	static const char * const clks[] = {
		/* cksys_mm */
		"mm_mmup_sel",
		"mm_mminfra_ao_sel",
		"mm_mminfra_sel",
		"mm_mminfra_snoc_sel",
		"mm_venc_sel",
		"mm_venc_mdp_sel",
		"mm_vdec_sel",
		"mm_img1_sel",
		"mm_ipe_sel",
		"mm_disp_sel",
		"mm_mml_sel",
		"mm_dvo_dp_sel",
		"mm_dvo_favt_dp_sel",
		"mm_cam_sel",
		"mm_camtm_sel",
		"mm_ccusys_sel",
		"mm_ccutm_sel",
		"mm_seninf0_sel",
		"mm_seninf1_sel",
		"mm_seninf2_sel",
		"mm_seninf3_sel",
		"mm_seninf4_sel",
		"mm_seninf5_sel",
		"mm_mminfra_snoc_slow_sel",

		/* mainpll2_ctrl */
		"mainpll2",

		/* univpll2_ctrl */
		"univpll2",

		/* mmpll_ctrl */
		"mmpll",

		/* imgpll_ctrl */
		"imgpll",

		/* tvdpll_ctrl */
		"tvdpll",

		/* imp_iic_wrap_e */
		"impe_i2c5",
		"impe_i2c2",
		"impe_i2c4",
		"impe_i2c7",
		"impe_i2c8",
		"impe_i2c11",

		/* imp_iic_wrap_s */
		"imps_i2c0",
		"imps_i2c3",
		"imps_i2c6",
		"imps_i2c10",

		/* imp_iic_wrap_n */
		"impn_i2c1",
		"impn_i2c9",

		/* apifrbus_ao_mem_reg */
		"ifrao_dpmaif_main",
		"ifrao_dpmaif_26m",

		/* imp_iic_wrap_c */
		"impc_i2c12",
		"impc_i2c13",
		"impc_i2c14",

		/* pericfg_ao */
		"perao_u_uart0_bclk",
		"perao_u_uart1_bclk",
		"perao_u_uart2_bclk",
		"perao_u_uart3_bclk",
		"perao_u_uart4_bclk",
		"perao_u_uart5_bclk",
		"perao_u_pwm_x16w",
		"perao_u_pwm_x16w_bclk",
		"perao_u_pwm_pwm_bclk0",
		"perao_u_pwm_pwm_bclk1",
		"perao_u_pwm_pwm_bclk2",
		"perao_u_pwm_pwm_bclk3",
		"perao_u_pwm_pwm_bclk4",
		"perao_u_pwm_pwm_bclk5",
		"perao_u_pwm_pwm_bclk6",
		"perao_u_pwm_pwm_bclk7",
		"perao_u_pwm_pwm_bclk8",
		"perao_u_pwm_pwm_bclk9",
		"perao_u_spi0_bclk",
		"perao_u_spi1_bclk",
		"perao_u_spi2_bclk",
		"perao_u_spi3_bclk",
		"perao_u_spi4_bclk",
		"perao_u_spi5_bclk",
		"perao_u_spi6_bclk",
		"perao_u_spi7_bclk",
		"perao_u_ap_dma_x32w_bclk",
		"perao_u_msdc1_msdc_src",
		"perao_u_msdc1",
		"perao_u_msdc1_axi",
		"perao_u_msdc1_h_wrap",
		"perao_u_msdc2_msdc_src",
		"perao_u_msdc2",
		"perao_u_msdc2_axi",
		"perao_u_msdc2_h_wrap",

		/* usb0cfg_ao */
		"usb_ao_usb0_ssusb0_frmcnt",

		/* ufs0cfg_ao */
		"ufs0ao_unipro_tx_sym",
		"ufs0ao_unipro_rx_sym0",
		"ufs0ao_unipro_rx_sym1",
		"ufs0ao_unipro_sys",
		"ufs0ao_unipro_sap",
		"ufs0ao_u_phy_sap",
		"ufs0ao_u_phy_ahb_s_busck",
		"ufs0ao_ufshci_ufs",
		"ufs0ao_ufshci_aes",

		/* ufs1cfg_ao */
		"ufs1ao_unipro_tx_sym",
		"ufs1ao_unipro_rx_sym0",
		"ufs1ao_unipro_rx_sym1",
		"ufs1ao_unipro_sys",
		"ufs1ao_unipro_sap",
		"ufs1ao_u_phy_sap",
		"ufs1ao_u_phy_ahb_s_busck",
		"ufs1ao_ufshci_ufs",
		"ufs1ao_ufshci_aes",

		/* pextp0cfg_ao */
		"pextp0_mac0_tl",
		"pextp0_mac0_ref",
		"pextp0_phy0_mcu_bus",
		"pextp0_phy0_pextp_ref",
		"pextp0_mac0_axi_250",
		"pextp0_mac0_ahb_apb",
		"pextp0_mac0_pl_p",
		"pextp0_vlp0_lp",

		/* pextp1cfg_ao */
		"pextp1_mac1_tl",
		"pextp1_mac1_ref",
		"pextp1_phy1_mcu_bus",
		"pextp1_phy1_pextp_ref",
		"pextp1_mac1_axi_250",
		"pextp1_mac1_ahb_apb",
		"pextp1_mac1_pl_p",
		"pextp1_vlp1_lp",

		/* scp_i3c */
		"scp_i3c_i2c0",
		"scp_i3c_i2c1",
		"scp_i3c_i2c2",
		"scp_i3c_i2c3",
		"scp_i3c_i2c4",
		"scp_i3c_i2c5",
		"scp_i3c_i2c6",
		"scp_i3c_i2c7",

		/* scp_fast_i3c */
		"scp_fast_i3c_0",
		"scp_fast_i3c_1",
		"scp_fast_i3c_2",

		/* afe */
		"afe_pcm1",
		"afe_pcm0",
		"afe_cm2",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_fm_i2s",
		"afe_mtkaifv4",
		"afe_vowif",
		"afe_ul2_aht",
		"afe_ul2_adc_hires",
		"afe_ul2_tml",
		"afe_ul2_adc",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in6",
		"afe_etdm_in5",
		"afe_etdm_in4",
		"afe_etdm_in3",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out5",
		"afe_etdm_out4",
		"afe_etdm_out3",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
		"afe_tdm_out",
		"afe_general15_asrc",
		"afe_general14_asrc",
		"afe_general13_asrc",
		"afe_general12_asrc",
		"afe_general11_asrc",
		"afe_general10_asrc",
		"afe_general9_asrc",
		"afe_general8_asrc",
		"afe_general7_asrc",
		"afe_general6_asrc",
		"afe_general5_asrc",
		"afe_general4_asrc",
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

		/* cksys_vlp */
		"vlp_sspm_26m_sel",
		"vlp_ulposc_sspm_sel",
		"vlp_sspm_sel",
		"vlp_spm_sel",
		"vlp_axi_vlp_sel",
		"vlp_noc_vlp_sel",
		"vlp_pwm_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_dpsw_sel",
		"vlp_dpsw_central_sel",
		"vlp_srck_sel",
		"vlp_dvfsrc_sel",
		"vlp_dbg_err_vlp_26m_sel",
		"vlp_ips_sel",
		"vlp_dpmsrdma_sel",
		"vlp_vlp_pbus_sel",
		"vlp_vlp_pbus_26m_sel",
		"vlp_vcore_pbus_sel",
		"vlp_vcore_pbus_26m_sel",
		"vlp_camtg0_sel",
		"vlp_camtg1_sel",
		"vlp_camtg2_sel",
		"vlp_camtg3_sel",
		"vlp_camtg4_sel",
		"vlp_camtg5_sel",
		"vlp_camtg6_sel",
		"vlp_camtg7_sel",
		"vlp_aud_engen1_sel",
		"vlp_aud_engen2_sel",
		"vlp_aud_sw_engen1_sel",
		"vlp_aud_sw_engen2_sel",
		"vlp_aud_intbus_sel",
		"vlp_audio_h_sel",
		"vlp_usb_sel",
		"vlp_usb_xhci_sel",
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_usb_mem_vlp_sel",
		"vlp_disp_pwm_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_tia_sel",
		"vlp_spmi_m_sel",
		"vlp_hvs_sel",

		/* apll1_ctrl */
		"apll1",

		/* apll2_ctrl */
		"apll2",

		/* cksys_top */
		"cksys_axi_sel",
		"cksys_peri_axi_sel",
		"cksys_ch_infra_axi_sel",
		"cksys_ch_infra_sel",
		"cksys_mem_sub_sel",
		"cksys_hash_sub_sel",
		"cksys_peri_fmem_sub_sel",
		"cksys_zram_sub_sel",
		"cksys_io_noc_sel",
		"cksys_hash_noc_sel",
		"cksys_peri_noc_sel",
		"cksys_md_emi_sel",
		"cksys_emi_n_sel",
		"cksys_emi_s_sel",
		"cksys_emi_infra_sel",
		"cksys_emi_infra_sspm_sel",
		"cksys_osc_emi_ifr_sel",
		"cksys_emi_infra_26m_sel",
		"cksys_infra_26m_sel",
		"cksys_cbus_phy_sel",
		"cksys_atb_sel",
		"cksys_cirq_sel",
		"cksys_mcu_infra_sel",
		"cksys_apu_ext_sel",
		"cksys_ssr_rng_sel",
		"cksys_efuse_sel",
		"cksys_dpsw_cmp_26m_sel",
		"cksys_adsp_uarthub_b_sel",
		"cksys_aud_1_sel",
		"cksys_aud_2_sel",
		"cksys_dpmaif_main_sel",
		"cksys_ipseast_sel",
		"cksys_ipswest_sel",
		"cksys_smapck_sel",
		"cksys_ipic_sel",
		"cksys_spi0_b_sel",
		"cksys_spi1_b_sel",
		"cksys_spi2_b_sel",
		"cksys_spi3_b_sel",
		"cksys_spi4_b_sel",
		"cksys_spi5_b_sel",
		"cksys_spi6_b_sel",
		"cksys_spi7_b_sel",
		"cksys_tl_sel",
		"cksys_tl_p1_sel",
		"cksys_pwm_sel",
		"cksys_aes_ufsfde_0_sel",
		"cksys_u_0_sel",
		"cksys_aes_ufsfde_1_sel",
		"cksys_u_1_sel",
		"cksys_uarthub_b_sel",
		"cksys_uart_sel",
		"cksys_i2c_peri_sel",
		"cksys_i2c_north_sel",
		"cksys_i2c_east_sel",
		"cksys_i2c_west_sel",
		"cksys_msdc_macro_1p_sel",
		"cksys_msdc_macro_2p_sel",
		"cksys_msdc30_1_sel",
		"cksys_msdc30_2_sel",
		"cksys_gridsensor_sel",
		"cksys_aov_26m_sel",
		"cksys_emi_wdat_sel",
		"cksys_apll_i2sin0_m_sel",
		"cksys_apll_i2sin1_m_sel",
		"cksys_apll_i2sin2_m_sel",
		"cksys_apll_i2sin3_m_sel",
		"cksys_apll_i2sin4_m_sel",
		"cksys_apll_i2sin6_m_sel",
		"cksys_apll_i2sout0_m_sel",
		"cksys_apll_i2sout1_m_sel",
		"cksys_apll_i2sout2_m_sel",
		"cksys_apll_i2sout3_m_sel",
		"cksys_apll_i2sout4_m_sel",
		"cksys_apll_i2sout6_m_sel",
		"cksys_apll_fmi2s_m_sel",
		"cksys_apll_tdmout_m_sel",

		/* cksys_top */
		"cksys_apll12_div_i2sin0",
		"cksys_apll12_div_i2sin1",
		"cksys_apll12_div_i2sin2",
		"cksys_apll12_div_i2sin3",
		"cksys_apll12_div_i2sin4",
		"cksys_apll12_div_i2sin6",
		"cksys_apll12_div_i2sout0",
		"cksys_apll12_div_i2sout1",
		"cksys_apll12_div_i2sout2",
		"cksys_apll12_div_i2sout3",
		"cksys_apll12_div_i2sout4",
		"cksys_apll12_div_i2sout6",
		"cksys_apll12_div_fmi2s",
		"cksys_apll12_div_tdmout_m",
		"cksys_apll12_div_tdmout_b",

		/* mainpll_ctrl */
		"mainpll",

		/* univpll_ctrl */
		"univpll",

		/* msdcpll_ctrl */
		"msdcpll",

		/* emipll_ctrl */
		"emipll",

		/* vdisp_dvfsrc_apb */
		"vdisp_rc_dvfsrc_en",

		/* mmlsys_config */
		"mml_mdp_mutex0",
		"mml_smi0",
		"mml_apb_bus",
		"mml_mdp_rdma2",
		"mml_mdp_birsz0",
		"mml_mdp_hdr0",
		"mml_mdp_aal0",
		"mml_mdp_tdshp0",
		"mml_mdp_color0",
		"mml_mdp_wrot2",
		"mml_mdp_fake_eng0",
		"mml_mdp_fake_eng1",
		"mml_apb_db",
		"mml_mdp_dli_as0",
		"mml_mdp_dlo_as0",
		"mml_mdp_c3d0",
		"mml_mdp_fg0",
		"mml_mdp_rsz2",
		"mml_mdp_rsz3",
		"mml_mdp_disp_chist0",
		"mml_disp_dbg",

		/* mml1_mmlsys_config */
		"mml1_mdp_mutex0",
		"mml1_smi0",
		"mml1_apb_bus",
		"mml1_mdp_rdma2",
		"mml1_mdp_birsz0",
		"mml1_mdp_hdr0",
		"mml1_mdp_aal0",
		"mml1_mdp_tdshp0",
		"mml1_mdp_color0",
		"mml1_mdp_wrot2",
		"mml1_mdp_fake_eng0",
		"mml1_mdp_fake_eng1",
		"mml1_apb_db",
		"mml1_mdp_dli_as0",
		"mml1_mdp_dlo_as0",
		"mml1_mdp_c3d0",
		"mml1_mdp_fg0",
		"mml1_mdp_rsz2",
		"mml1_mdp_rsz3",
		"mml1_mdp_disp_chist0",
		"mml1_disp_dbg",

		/* mml2_mmlsys_config */
		"mml2_mdp_mutex0",
		"mml2_smi0",
		"mml2_apb_bus",
		"mml2_mdp_rdma0",
		"mml2_mdp_rsz0",
		"mml2_mdp_rsz1",
		"mml2_mdp_wrot0",
		"mml2_mdp_fake_eng0",
		"mml2_mdp_fake_eng1",
		"mml2_apb_db",
		"mml2_mdp_dli_as0",
		"mml2_mdp_dli_as1",
		"mml2_mdp_dlo_as0",
		"mml2_mdp_dlo_as1",
		"mml2_mdp_dli_as2",
		"mml2_mdp_dlo_as2",
		"mml2_mdp_dlo_as3",
		"mml2_mdp_rrot0",
		"mml2_mdp_merge0",
		"mml2_mdp_dlo_as4",
		"mml2_mdp_dlo_as5",
		"mml2_mdp_rdma1",
		"mml2_mdp_rrot1",
		"mml2_mdp_wrot1",
		"mml2_mdp_disp_wdma0",
		"mml2_mdp_disp_wdma1",
		"mml2_mdp_dli_as3",
		"mml2_mdp_dli_as4",
		"mml2_disp_dbg",

		/* ovlsys_config */
		"ovlsys_config",
		"ovl_mutex0",
		"ovl_exdma0",
		"ovl_exdma1",
		"ovl_exdma2",
		"ovl_exdma3",
		"ovl_exdma4",
		"ovl_exdma5",
		"ovl_exdma6",
		"ovl_exdma7",
		"ovl_blender0",
		"ovl_blender1",
		"ovl_blender2",
		"ovl_blender3",
		"ovl_blender4",
		"ovl_blender5",
		"ovl_blender6",
		"ovl_blender7",
		"ovl_outproc0",
		"ovl_outproc1",
		"ovl_outproc2",
		"ovl_outproc3",
		"ovl_mdp_rsz0",
		"ovl_odpm0",
		"ovl_inlinerot0",
		"ovl_fake_eng0",
		"ovl_fake_eng1",
		"ovl_fake_eng2",
		"ovl_fake_eng3",
		"ovl_dli_as0",
		"ovl_dli_as1",
		"ovl_dli_as2",
		"ovl_dli_as3",
		"ovl_dli_as4",
		"ovl_dli_as5",
		"ovl_dli_as6",
		"ovl_dli_as7",
		"ovl_dli_as8",
		"ovl_dli_as9",
		"ovl_dli_as10",
		"ovl_dli_as11",
		"ovl_dli_as12",
		"ovl_dlo_as0",
		"ovl_dlo_as1",
		"ovl_dlo_as2",
		"ovl_dlo_as3",
		"ovl_dlo_as4",
		"ovl_dlo_as5",
		"ovl_dlo_as6",
		"ovl_dlo_as7",
		"ovl_dlo_as8",
		"ovl_dlo_as9",
		"ovl_dlo_as10",
		"ovl_dlo_as11",
		"ovl_dlo_as12",
		"ovl_dlo_as13",
		"ovl_dlo_as14",
		"ovl_dlo_as15",
		"ovlsys_relay0",
		"ovl_ssc",
		"ovl_disp_dbg",

		/* ovl1_ovlsys_config */
		"ovl1_ovlsys_config",
		"ovl1_ovl_mutex0",
		"ovl1_ovl_exdma0",
		"ovl1_ovl_exdma1",
		"ovl1_ovl_exdma2",
		"ovl1_ovl_exdma3",
		"ovl1_ovl_exdma4",
		"ovl1_ovl_exdma5",
		"ovl1_ovl_exdma6",
		"ovl1_ovl_exdma7",
		"ovl1_ovl_blender0",
		"ovl1_ovl_blender1",
		"ovl1_ovl_blender2",
		"ovl1_ovl_blender3",
		"ovl1_ovl_blender4",
		"ovl1_ovl_blender5",
		"ovl1_ovl_blender6",
		"ovl1_ovl_blender7",
		"ovl1_ovl_outproc0",
		"ovl1_ovl_outproc1",
		"ovl1_ovl_outproc2",
		"ovl1_ovl_outproc3",
		"ovl1_ovl_mdp_rsz0",
		"ovl1_ovl_odpm0",
		"ovl1_inlinerot0",
		"ovl1_ovl_fake_eng0",
		"ovl1_ovl_fake_eng1",
		"ovl1_ovl_fake_eng2",
		"ovl1_ovl_fake_eng3",
		"ovl1_ovl_dli_as0",
		"ovl1_ovl_dli_as1",
		"ovl1_ovl_dli_as2",
		"ovl1_ovl_dli_as3",
		"ovl1_ovl_dli_as4",
		"ovl1_ovl_dli_as5",
		"ovl1_ovl_dli_as6",
		"ovl1_ovl_dli_as7",
		"ovl1_ovl_dli_as8",
		"ovl1_ovl_dli_as9",
		"ovl1_ovl_dli_as10",
		"ovl1_ovl_dli_as11",
		"ovl1_ovl_dli_as12",
		"ovl1_ovl_dlo_as0",
		"ovl1_ovl_dlo_as1",
		"ovl1_ovl_dlo_as2",
		"ovl1_ovl_dlo_as3",
		"ovl1_ovl_dlo_as4",
		"ovl1_ovl_dlo_as5",
		"ovl1_ovl_dlo_as6",
		"ovl1_ovl_dlo_as7",
		"ovl1_ovl_dlo_as8",
		"ovl1_ovl_dlo_as9",
		"ovl1_ovl_dlo_as10",
		"ovl1_ovl_dlo_as11",
		"ovl1_ovl_dlo_as12",
		"ovl1_ovl_dlo_as13",
		"ovl1_ovl_dlo_as14",
		"ovl1_ovl_dlo_as15",
		"ovl1_ovlsys_relay0",
		"ovl1_ssc",
		"ovl1_disp_dbg",

		/* imgsys_main */
		"img_fdvt",
		"img_larb12",
		"img_ipesys_cvfs26",
		"img_odpm26",
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
		"img_sdl0",
		"img_sdl1",
		"img_sdl2",
		"img_sdl3",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common2",
		"img_sub_common3",
		"img_sub_common4",
		"img_sub_common5",
		"img_sub_common6",
		"img_gals7",
		"img_gals",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_wpe2",
		"img_gals_rx_wpe3",
		"img_gals_trx_ipe0",
		"img_gals_trx_ipe1",
		"img26",
		"img_bwr",
		"img_isc",
		"imgsys_cvfs26",

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

		/* dip_cine_dip1 */
		"dip_cine_dip1_larb",
		"dip_cine_dip1_dip_cine",

		/* wpe_eis_dip1 */
		"wpe_eis_dip1_larb_u0",
		"wpe_eis_dip1_larb_u1",
		"wpe_eis_dip1_gals_u0",
		"wpe_eis_dip1_gals_u1",
		"wpe_eis_dip1_wpe_macro",
		"wpe_eis_dip1_wpe",
		"wpe_eis_dip1_pqdip",
		"wpe_eis_dip1_pqdip_dma",
		"wpe_eis_dip1_omc",
		"wpe_eis_dip1_dpe",
		"wpe_eis_dip1_dfps",
		"wpe_eis_dip1_dfp0",
		"wpe_eis_dip1_dfp1",
		"wpe_eis_dip1_dwpe",
		"wpe_eis_dip1_me",
		"wpe_eis_dip1_mmg",
		"wpe_eis_dip1_wpe_26m",

		/* wpe_tnr_dip1 */
		"wpe_tnr_dip1_larb_u0",
		"wpe_tnr_dip1_larb_u1",
		"wpe_tnr_dip1_gals_u0",
		"wpe_tnr_dip1_gals_u1",
		"wpe_tnr_dip1_wpe_macro",
		"wpe_tnr_dip1_wpe",
		"wpe_tnr_dip1_pqdip",
		"wpe_tnr_dip1_pqdip_dma",
		"wpe_tnr_dip1_omc",
		"wpe_tnr_dip1_dpe",
		"wpe_tnr_dip1_dfps",
		"wpe_tnr_dip1_dfp0",
		"wpe_tnr_dip1_dfp1",
		"wpe_tnr_dip1_dwpe",
		"wpe_tnr_dip1_me",
		"wpe_tnr_dip1_mmg",

		/* wpe_lite_dip1 */
		"wpe_lite_dip1_larb_u0",
		"wpe_lite_dip1_larb_u1",
		"wpe_lite_dip1_gals_u0",
		"wpe_lite_dip1_gals_u1",
		"wpe_lite_dip1_wpe_macro",
		"wpe_lite_dip1_wpe",
		"wpe_lite_dip1_pqdip",
		"wpe_lite_dip1_pqdip_dma",
		"wpe_lite_dip1_omc",
		"wpe_lite_dip1_dpe",
		"wpe_lite_dip1_dfps",
		"wpe_lite_dip1_dfp0",
		"wpe_lite_dip1_dfp1",
		"wpe_lite_dip1_dwpe",
		"wpe_lite_dip1_me",
		"wpe_lite_dip1_mmg",

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
		"img_vcore_sub2",
		"img_vcore_img_26m",

		/* venc_gcon_mdp */
		"ven_mdp_larb",
		"ven_mdp_jpgenc",
		"ven_mdp_jpgenc_c1",
		"ven_mdp_jpgdec",
		"ven_mdp_jpgdec_c1",
		"ven_mdp_jpgdec_c2",

		/* cam_main_r1a */
		"cam_m_larb27",
		"cam_m_cam",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_subc",
		"cam_m_cam_mraw",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_adlrd",
		"cam_m_adlwr",
		"cam_m_uisp",
		"cam_m_sdl_0c_0",
		"cam_m_sdl_1",
		"cam_m_larb27_gcon_0",
		"cam_m_cam2sys_gcon_0",
		"cam_m_ips",
		"cam_m_cam_asg",
		"cam_m_cam_qof_con_1",
		"cam_m_cam_bwr_con_1",
		"cam_m_cam_rtcq_con_1",
		"cam_m_cam_sdlcq_con_1",
		"cam_m_cam_wla_con_1",
		"cam_m_cam_dvc_con_1",
		"cam_m_cam_cvfs_con_1",

		/* camsys_mraw */
		"cam_mr_larb13",
		"cam_mr_larb14",
		"cam_mr_larb19",
		"cam_mr_larb25",
		"cam_mr_larb26",
		"cam_mr_larb29",
		"cam_mr_gals0",
		"cam_mr_gals1",
		"cam_mr_gals2",
		"cam_mr_gals3",
		"cam_mr_gals4",
		"cam_mr_gals5",
		"cam_mr_seninf_camtm",
		"cam_mr_camsv_top",
		"cam_mr_camsv_a",
		"cam_mr_camsv_b",
		"cam_mr_camsv_c",
		"cam_mr_camsv_d",
		"cam_mr_camsv_e",
		"cam_mr_camsv",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		"cam_ra_raw2mm_gals",
		"cam_ra_yuv2raw2mm",
		"cam_ra_cam_26m",

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
		"cam_rb_cam_26m",

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
		"cam_rc_cam_26m",

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
		"ccu2infra_gcon",
		"ccusys_ccu0_con",
		"ccu2mm0_gcon",

		/* cam_vcore_r1a */
		"cam_v_r1a_g",
		"cam_v_r1a_26m",
		"cam_v_r1a_bls_part",
		"cam_v_r1a_bls_full",
		"cam_v_r1a_sv0_gcon_0",
		"cam_v_r1a_sv1_gcon_0",
		"cam_v_r1a_tx6_gcon_0",
		"cam_v_r1a_tx13_gcon_0",
		"cam_v_r1a_tx47_gcon_0",
		"cam_v_r1a_tx16_gcon_0",
		"cam_v_r1a_tx22_gcon_0",
		"cam_v_r1a_tx90_gcon_0",
		"cam_v_r1a_sub_comm0",
		"cam_v_r1a_sub_comm1",
		"cam_v_r1a_sub_comm2",
		"cam_v_r1a_sub_comm3",
		"cam_v_r1a_x51_gcon_0",
		"cam_v_r1a_x50_gcon_0",
		"cam_v_r1a_x45_gcon_0",
		"cam_v_r1a_x43_gcon_0",
		"cam_v_r1a_x52_gcon_0",
		"cam_v_r1a_x44_gcon_0",
		"cam_v_r1a_x91_gcon_0",
		"cam_v_r1a_x54_gcon_0",
		"cam_v_r1a_x48_gcon_0",
		"cam_v_r1a_x93_gcon_0",
		"cam_v_r1a_x46_gcon_0",
		"cam_v_r1a_x53_gcon_0",
		"cam_v_r1a_x94_gcon_0",
		"cam_v_r1a_x22_gcon_0",
		"cam_v_r1a_x60_gcon_0",
		"cam_v_r1a_x90_gcon_0",
		"cam_v_r1a_gals51_con_1",
		"cam_v_r1a_gals50_con_1",
		"cam_v_r1a_gals45_con_1",
		"cam_v_r1a_gals43_con_1",
		"cam_v_r1a_gals52_con_1",
		"cam_v_r1a_gals44_con_1",
		"cam_v_r1a_gals91_con_1",
		"cam_v_r1a_gals54_con_1",
		"cam_v_r1a_gals48_con_1",
		"cam_v_r1a_gals93_con_1",
		"cam_v_r1a_gals46_con_1",
		"cam_v_r1a_gals53_con_1",
		"cam_v_r1a_gals94_con_1",
		"cam_v_r1a_gals22_con_1",

		/* ovl2_ovlsys_config */
		"ovl2_ovlsys_config",
		"ovl2_ovl_mutex0",
		"ovl2_ovl_exdma0",
		"ovl2_ovl_exdma1",
		"ovl2_ovl_exdma2",
		"ovl2_ovl_exdma3",
		"ovl2_ovl_exdma4",
		"ovl2_ovl_exdma5",
		"ovl2_ovl_exdma6",
		"ovl2_ovl_exdma7",
		"ovl2_ovl_blender0",
		"ovl2_ovl_blender1",
		"ovl2_ovl_blender2",
		"ovl2_ovl_blender3",
		"ovl2_ovl_blender4",
		"ovl2_ovl_blender5",
		"ovl2_ovl_blender6",
		"ovl2_ovl_blender7",
		"ovl2_ovl_outproc0",
		"ovl2_ovl_outproc1",
		"ovl2_ovl_outproc2",
		"ovl2_ovl_outproc3",
		"ovl2_ovl_mdp_rsz0",
		"ovl2_ovl_odpm0",
		"ovl2_inlinerot0",
		"ovl2_ovl_fake_eng0",
		"ovl2_ovl_fake_eng1",
		"ovl2_ovl_fake_eng2",
		"ovl2_ovl_fake_eng3",
		"ovl2_ovl_dli_as0",
		"ovl2_ovl_dli_as1",
		"ovl2_ovl_dli_as2",
		"ovl2_ovl_dli_as3",
		"ovl2_ovl_dli_as4",
		"ovl2_ovl_dli_as5",
		"ovl2_ovl_dli_as6",
		"ovl2_ovl_dli_as7",
		"ovl2_ovl_dli_as8",
		"ovl2_ovl_dli_as9",
		"ovl2_ovl_dli_as10",
		"ovl2_ovl_dli_as11",
		"ovl2_ovl_dli_as12",
		"ovl2_ovl_dlo_as0",
		"ovl2_ovl_dlo_as1",
		"ovl2_ovl_dlo_as2",
		"ovl2_ovl_dlo_as3",
		"ovl2_ovl_dlo_as4",
		"ovl2_ovl_dlo_as5",
		"ovl2_ovl_dlo_as6",
		"ovl2_ovl_dlo_as7",
		"ovl2_ovl_dlo_as8",
		"ovl2_ovl_dlo_as9",
		"ovl2_ovl_dlo_as10",
		"ovl2_ovl_dlo_as11",
		"ovl2_ovl_dlo_as12",
		"ovl2_ovl_dlo_as13",
		"ovl2_ovl_dlo_as14",
		"ovl2_ovl_dlo_as15",
		"ovl2_ovlsys_relay0",
		"ovl2_ssc",
		"ovl2_disp_dbg",

		/* dispsys_config */
		"mm0_config",
		"mm0_disp_mutex0",
		"mm0_disp_aal0",
		"mm0_disp_c3d0",
		"mm0_disp_c3d1",
		"mm0_disp_ccorr0",
		"mm0_disp_ccorr1",
		"mm0_disp_color0",
		"mm0_disp_dither0",
		"mm0_disp_dither1",
		"mm0_disp_dli_as0",
		"mm0_disp_dli_as1",
		"mm0_disp_dli_as2",
		"mm0_disp_dli_as3",
		"mm0_disp_dli_as4",
		"mm0_disp_dli_as5",
		"mm0_disp_dli_as6",
		"mm0_disp_dli_as7",
		"mm0_disp_dli_as8",
		"mm0_disp_dli_as9",
		"mm0_disp_dli_as10",
		"mm0_disp_dli_as11",
		"mm0_disp_dli_as12",
		"mm0_disp_dli_as13",
		"mm0_disp_dli_as14",
		"mm0_disp_dli_as15",
		"mm0_disp_dlo_as0",
		"mm0_disp_dlo_as1",
		"mm0_disp_dlo_as2",
		"mm0_disp_dlo_as3",
		"mm0_disp_dlo_as4",
		"mm0_disp_dlo_as5",
		"mm0_disp_dlo_as6",
		"mm0_disp_dlo_as7",
		"mm0_disp_dlo_as8",
		"mm0_disp_dlo_as9",
		"mm0_disp_dlo_as10",
		"mm0_disp_dlo_as11",
		"mm0_disp_dlo_as12",
		"mm0_disp_dlo_as13",
		"mm0_disp_dlo_as14",
		"mm0_disp_dlo_as15",
		"mm0_disp_dlo_as16",
		"mm0_disp_dlo_as17",
		"mm0_disp_dlo_as18",
		"mm0_disp_dlo_as19",
		"mm0_disp_dlo_as20",
		"mm0_disp_relay5",
		"mm0_disp_gamma0",
		"mm0_mdp_aal0",
		"mm0_disp_postmask0",
		"mm0_mdp_rdma0",
		"mm0_disp_spr0",
		"mm0_disp_oddmr0",
		"mm0_mdp_rsz0",
		"mm0_disp_tdshp0",
		"mm0_ssc",
		"mm0_disp_fake_eng0",
		"mm0_disp_dbg",

		/* disp0b_dispsys_config */
		"mm0b_config",
		"mm0b_disp_mutex0",
		"mm0b_disp_aal0",
		"mm0b_disp_c3d0",
		"mm0b_disp_c3d1",
		"mm0b_disp_ccorr0",
		"mm0b_disp_ccorr1",
		"mm0b_disp_color0",
		"mm0b_disp_dither0",
		"mm0b_disp_dither1",
		"mm0b_disp_dli_as0",
		"mm0b_disp_dli_as1",
		"mm0b_disp_dli_as2",
		"mm0b_disp_dli_as3",
		"mm0b_disp_dli_as4",
		"mm0b_disp_dli_as5",
		"mm0b_disp_dli_as6",
		"mm0b_disp_dli_as7",
		"mm0b_disp_dli_as8",
		"mm0b_disp_dli_as9",
		"mm0b_disp_dli_as10",
		"mm0b_disp_dli_as11",
		"mm0b_disp_dli_as12",
		"mm0b_disp_dli_as13",
		"mm0b_disp_dli_as14",
		"mm0b_disp_dli_as15",
		"mm0b_disp_dlo_as0",
		"mm0b_disp_dlo_as1",
		"mm0b_disp_dlo_as2",
		"mm0b_disp_dlo_as3",
		"mm0b_disp_dlo_as4",
		"mm0b_disp_dlo_as5",
		"mm0b_disp_dlo_as6",
		"mm0b_disp_dlo_as7",
		"mm0b_disp_dlo_as8",
		"mm0b_disp_dlo_as9",
		"mm0b_disp_dlo_as10",
		"mm0b_disp_dlo_as11",
		"mm0b_disp_dlo_as12",
		"mm0b_disp_dlo_as13",
		"mm0b_disp_dlo_as14",
		"mm0b_disp_dlo_as15",
		"mm0b_disp_dlo_as16",
		"mm0b_disp_dlo_as17",
		"mm0b_disp_dlo_as18",
		"mm0b_disp_dlo_as19",
		"mm0b_disp_dlo_as20",
		"mm0b_disp_relay5",
		"mm0b_disp_gamma0",
		"mm0b_mdp_aal0",
		"mm0b_disp_postmask0",
		"mm0b_mdp_rdma0",
		"mm0b_disp_spr0",
		"mm0b_disp_oddmr0",
		"mm0b_mdp_rsz0",
		"mm0b_disp_tdshp0",
		"mm0b_ssc",
		"mm0b_disp_fake_eng0",
		"mm0b_disp_dbg",

		/* dispsys1_config */
		"mm1_disp1_cfg",
		"mm1_disp1_s_cfg",
		"mm1_disp_mutex0",
		"mm1_disp_dli_as20",
		"mm1_disp_dli_as21",
		"mm1_disp_dli_as22",
		"mm1_disp_dli_as23",
		"mm1_disp_dli_as24",
		"mm1_disp_dli_as25",
		"mm1_disp_dli_as26",
		"mm1_disp_dli_as27",
		"mm1_disp_dli_as28",
		"mm1_disp_dli_as29",
		"mm1_disp_dli_as30",
		"mm1_disp_dli_as31",
		"mm1_disp_dli_as32",
		"mm1_disp_dli_as33",
		"mm1_disp_dli_as34",
		"mm1_disp_dli_as35",
		"mm1_disp_dli_as36",
		"mm1_disp_dli_as37",
		"mm1_disp_dli_as38",
		"mm1_disp_dlo_as31",
		"mm1_disp_dlo_as32",
		"mm1_disp_dlo_as33",
		"mm1_disp_dlo_as34",
		"mm1_disp_dlo_as35",
		"mm1_disp_dlo_as36",
		"mm1_disp_dlo_as37",
		"mm1_disp_dlo_as38",
		"mm1_disp_relay0",
		"mm1_disp_relay1",
		"mm1_disp_relay2",
		"mm1_disp_relay3",
		"mm1_disp_relay4",
		"mm1_0",
		"mm1_1",
		"mm1_disp_dvo0",
		"mm1_disp_wdma0",
		"mm1_disp_wdma1",
		"mm1_disp_dbi_count0",
		"mm1_disp_chist0",
		"mm1_disp_chist1",
		"mm1_disp_chist2",
		"mm1_disp_postalign0",
		"mm1_disp_splitter0",
		"mm1_disp_splitter1",
		"mm1_disp_dsc_wrap0",
		"mm1_disp_dsc_wrap1",
		"mm1_disp_r2y0",
		"mm1_disp_gdma0",
		"mm1_disp_merge0",
		"mm1_disp_merge1",
		"mm1_smi_larb0",
		"mm1_disp_fake_eng1",
		"mm1_disp_dbg",

		/* disp1b_dispsys1_config */
		"mm1b_disp1_cfg",
		"mm1b_disp1_s_cfg",
		"mm1b_disp_mutex0",
		"mm1b_disp_dli_as20",
		"mm1b_disp_dli_as21",
		"mm1b_disp_dli_as22",
		"mm1b_disp_dli_as23",
		"mm1b_disp_dli_as24",
		"mm1b_disp_dli_as25",
		"mm1b_disp_dli_as26",
		"mm1b_disp_dli_as27",
		"mm1b_disp_dli_as28",
		"mm1b_disp_dli_as29",
		"mm1b_disp_dli_as30",
		"mm1b_disp_dli_as31",
		"mm1b_disp_dli_as32",
		"mm1b_disp_dli_as33",
		"mm1b_disp_dli_as34",
		"mm1b_disp_dli_as35",
		"mm1b_disp_dli_as36",
		"mm1b_disp_dli_as37",
		"mm1b_disp_dli_as38",
		"mm1b_disp_dlo_as31",
		"mm1b_disp_dlo_as32",
		"mm1b_disp_dlo_as33",
		"mm1b_disp_dlo_as34",
		"mm1b_disp_dlo_as35",
		"mm1b_disp_dlo_as36",
		"mm1b_disp_dlo_as37",
		"mm1b_disp_dlo_as38",
		"mm1b_disp_relay0",
		"mm1b_disp_relay1",
		"mm1b_disp_relay2",
		"mm1b_disp_relay3",
		"mm1b_disp_relay4",
		"mm1b_0",
		"mm1b_1",
		"mm1b_disp_dvo0",
		"mm1b_disp_wdma0",
		"mm1b_disp_wdma1",
		"mm1b_disp_dbi_count0",
		"mm1b_disp_chist0",
		"mm1b_disp_chist1",
		"mm1b_disp_chist2",
		"mm1b_disp_postalign0",
		"mm1b_disp_splitter0",
		"mm1b_disp_splitter1",
		"mm1b_disp_dsc_wrap0",
		"mm1b_disp_dsc_wrap1",
		"mm1b_disp_r2y0",
		"mm1b_disp_gdma0",
		"mm1b_disp_merge0",
		"mm1b_disp_merge1",
		"mm1b_smi_larb0",
		"mm1b_disp_fake_eng1",
		"mm1b_disp_dbg",

		/* disp_vdisp_ao_config */
		"vdisp_ao_config",
		"vdisp_ao_disp_pwm0",
		"vdisp_ao_disp_pwm1",
		"vdisp_ao_disp_dpc",
		"vdisp_ao_disp_bus",
		"vdisp_ao_disp_bwm0",
		"vdisp_ao_disp_bwm1",
		"vdisp_ao_disp_bwr",
		"vdisp_ao_disp_debug_top",
		"vdisp_ao_disp_cvfs",


        NULL
	};

	return clks;
}

/*
 * clkdbg test tasks
 */
static int clkdbg_thread_fn(void *data)
{
	struct test_clk t_clks[TEST_CLK_NUM] = {0};
	struct test_task_clk *test_clk = (struct test_task_clk *)data;
	struct clk *tmp_clk, *tmp_clk_p;

	int i;
	int ret = 0, test_clk_num;
	unsigned int thread_cnt = 0;
	int test_type;

	if (test_clk == NULL || test_clk->test_clk_num == 0) {
		pr_notice("clkdbg_thread receive NULL data\n");
		goto ERR;
	}

	test_type = test_clk->type;

	test_clk_num = test_clk->test_clk_num < TEST_CLK_NUM ?
		test_clk->test_clk_num : TEST_CLK_NUM;

	for (i = 0; i < test_clk_num; i++)
		t_clks[i] = test_clk->test_clk[i];

	while (!kthread_should_stop()) {
		if ((thread_cnt % 10000) == 0)
			pr_notice("%s is running...(%d)\n", current->comm, thread_cnt);

		switch (test_type) {
		case CLK_TEST_TASK_ON_OFF:
			/* Power on phase */
			for (i = 0; i < test_clk_num; i++) {
				switch (t_clks[i].test_clk_type) {
				case TEST_TYPE_CLK:
					tmp_clk = TEST_CLK_TO_CLK(t_clks[i]);
					ret = clk_prepare_enable(tmp_clk);
					if (ret < 0) {
						pr_notice("%s fail to power on(%d)\n",
							clk_hw_get_name(__clk_get_hw(tmp_clk)),
							ret);
						goto ERR;
					}
					break;

				}
			}
			/* Power off phase */
			for (i = test_clk_num - 1; i >= 0; i--) {
				switch (t_clks[i].test_clk_type) {
				case TEST_TYPE_CLK:
					tmp_clk = TEST_CLK_TO_CLK(t_clks[i]);
					clk_disable_unprepare(tmp_clk);
					break;

				}
			}
			break;
		case CLK_TEST_TASK_SET_PARENT:
			tmp_clk = TEST_CLK_TO_CLK(t_clks[0]);
			tmp_clk_p = clk_get_parent(tmp_clk);
			for (i = 1; i < test_clk_num; i++) {
				ret = clk_set_parent(tmp_clk,
						     TEST_CLK_TO_CLK(t_clks[i]));
				if (ret) {
					pr_notice("%s: %s, %s fail to set parent(%d)\n",
						current->comm,
						clk_hw_get_name(__clk_get_hw(tmp_clk)),
						clk_hw_get_name(
							__clk_get_hw(TEST_CLK_TO_CLK(t_clks[i]))),
						ret);
					/* reset to original parent */
					ret = clk_set_parent(tmp_clk, tmp_clk_p);
					if (ret)
						pr_notice("%s: %s, %s fail to set parent(%d)\n",
							current->comm,
							clk_hw_get_name(__clk_get_hw(tmp_clk)),
							clk_hw_get_name(__clk_get_hw(tmp_clk_p)),
						ret);
					goto ERR;
				}
			}
			/* reset to original parent */
			ret = clk_set_parent(tmp_clk, tmp_clk_p);
			if (ret)
				pr_notice("%s: %s, %s fail to set parent(%d)\n",
					current->comm,
					clk_hw_get_name(__clk_get_hw(tmp_clk)),
					clk_hw_get_name(__clk_get_hw(tmp_clk_p)),
				ret);
			break;
		default:
			pr_notice("unknown test type\n");
			goto ERR;
		}
		thread_cnt++;

		if (test_clk->repeat_time != -1 &&
		    (unsigned int)test_clk->repeat_time == thread_cnt)
			break;

		// add ~20ms delay to avoid mmup receiving too many irq
		if (test_type == CLK_TEST_TASK_ON_OFF)
			usleep_range(20000, 20100);
	}
ERR:
	pr_notice("%s stopped after (%d) run\n", current->comm, thread_cnt);
	kfree(data);
	return 0;
}

static int start_clkdbg_test_task(void *data)
{
	char thread_name[THREAD_LEN];
	struct task_struct *clkdbg_test_thread;
	int ret = 0;

	ret = snprintf(thread_name, THREAD_LEN, "clkdbg_thread%d", clkdbg_thread_cnt);

	if (ret < 0) {
		pr_info("%s snprintf error(%d)\n", __func__, ret);
		return ret;
	}

	clkdbg_test_thread = kthread_run(clkdbg_thread_fn, data, "%s", thread_name);
	if (IS_ERR(clkdbg_test_thread)) {
		pr_info("%s Failed to start clkdbg_thread(%d)\n", __func__, clkdbg_thread_cnt);
		return PTR_ERR(clkdbg_test_thread);
	}
	clkdbg_thread_cnt++;

	return 0;
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
	const struct fmeter_clk *fclks_base = get_all_fmeter_clks();
	int index = fclk - fclks_base;

	return mt_get_fmeter_freq(index, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6993_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6993_all_clk_names,
    .start_task = start_clkdbg_test_task,
};

static int clk_dbg_mt6993_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6993_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6993_drv = {
	.probe = clk_dbg_mt6993_probe,
	.driver = {
		.name = "clk-dbg-mt6993",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6993_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6993_drv, "clk-dbg-mt6993");
}

static void __exit clkdbg_mt6993_exit(void)
{
    unset_clkdbg_ops();
	platform_driver_unregister(&clk_dbg_mt6993_drv);
}

subsys_initcall(clkdbg_mt6993_init);
module_exit(clkdbg_mt6993_exit);
MODULE_LICENSE("GPL");
