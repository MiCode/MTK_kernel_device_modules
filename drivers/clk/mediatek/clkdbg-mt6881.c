// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
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
#include "clkchk-mt6881.h"
#include "clk-fmeter.h"

#define THREAD_LEN      (16)
#define THREAD_NUM      (6)
#define PD_NUM          (3)

static int clkdbg_thread_cnt;

const char * const *get_mt6881_all_clk_names(void)
{
	static const char * const clks[] = {
		/* cksys_reg */
		"cksys_axi_sel",
		"cksys_axi_peri_sel",
		"cksys_axi_ufs_sel",
		"cksys_bus_aximem_sel",
		"cksys_disp0_sel",
		"cksys_mminfra_sel",
		"cksys_mmup_sel",
		"cksys_uart_sel",
		"cksys_uart3_sel",
		"cksys_uarthub_b_sel",
		"cksys_spi0_sel",
		"cksys_spi1_sel",
		"cksys_spi2_sel",
		"cksys_spi3_sel",
		"cksys_spi4_sel",
		"cksys_spi5_sel",
		"cksys_spi6_sel",
		"cksys_spi7_sel",
		"cksys_msdc_macro_1p_sel",
		"cksys_msdc30_1_sel",
		"cksys_aud_intbus_sel",
		"cksys_atb_sel",
		"cksys_disp_pwm_sel",
		"cksys_usb_sel",
		"cksys_usb_xhci_sel",
		"cksys_i2c_sel",
		"cksys_i2c_5_sel",
		"cksys_seninf_sel",
		"cksys_seninf1_sel",
		"cksys_seninf2_sel",
		"cksys_seninf3_sel",
		"cksys_aud_engen1_sel",
		"cksys_aud_engen2_sel",
		"cksys_aes_ufsfde_sel",
		"cksys_ufs_sel",
		"cksys_ufs_mbist_sel",
		"cksys_aud_1_sel",
		"cksys_aud_2_sel",
		"cksys_dpmaif_main_sel",
		"cksys_venc_sel",
		"cksys_vdec_sel",
		"cksys_pwm_sel",
		"cksys_audio_h_sel",
		"cksys_mcupm_sel",
		"cksys_mem_sub_sel",
		"cksys_mem_sub_peri_sel",
		"cksys_mem_sub_ufs_sel",
		"cksys_emisys_sel",
		"cksys_dsi_occ_sel",
		"cksys_ap2conn_host_sel",
		"cksys_img1_sel",
		"cksys_ipe_sel",
		"cksys_cam_sel",
		"cksys_camtm_sel",
		"cksys_msdc_1p_rx_sel",
		"cksys_dsp_sel",
		"cksys_md_emi_sel",
		"cksys_ssr_pka_sel",
		"cksys_ssr_dma_sel",
		"cksys_ssr_kdf_sel",
		"cksys_ssr_rng_sel",
		"cksys_ssr_pqc_sel",
		"cksys_mfg_ref_sel",
		"cksys_mfgsc_ref_sel",
		"cksys_mfg_eb_sel",
		"cksys_spis0_b_sel",
		"cksys_spis1_b_sel",
		"cksys_spis0_deglitch_sel",
		"cksys_spis1_deglitch_sel",
		"cksys_tl_sel",
		"cksys_pextp_mbist_sel",
		"cksys_usb_frmcnt_sel",
		"cksys_armpll_div_pll1_sel",
		"cksys_camtg0_sel",
		"cksys_camtg1_sel",
		"cksys_camtg2_sel",
		"cksys_camtg3_sel",
		"cksys_camtg4_sel",
		"cksys_camtg5_sel",
		"cksys_apll_in0_m_sel",
		"cksys_apll_in1_m_sel",
		"cksys_apll_in2_m_sel",
		"cksys_apll_in3_m_sel",
		"cksys_apll_in4_m_sel",
		"cksys_apll_in6_m_sel",
		"cksys_apll_out0_m_sel",
		"cksys_apll_out1_m_sel",
		"cksys_apll_out2_m_sel",
		"cksys_apll_out3_m_sel",
		"cksys_apll_out4_m_sel",
		"cksys_apll_out6_m_sel",
		"cksys_apll_fmi2s_m_sel",
		"cksys_apll_m_sel",

		/* cksys_reg */
		"cksys_apll12_div_in0",
		"cksys_apll12_div_in1",
		"cksys_apll12_div_in2",
		"cksys_apll12_div_in3",
		"cksys_apll12_div_in4",
		"cksys_apll12_div_in6",
		"cksys_apll12_div_out0",
		"cksys_apll12_div_out1",
		"cksys_apll12_div_out2",
		"cksys_apll12_div_out3",
		"cksys_apll12_div_out4",
		"cksys_apll12_div_out6",
		"cksys_apll12_div_fmi2s",

		/* cksys_reg */
		"cksys_apll12_div_in0_pdn",
		"cksys_apll12_div_in1_pdn",
		"cksys_apll12_div_in2_pdn",
		"cksys_apll12_div_in3_pdn",
		"cksys_apll12_div_in4_pdn",
		"cksys_apll12_div_in6_pdn",
		"cksys_apll12_div_out0_pdn",
		"cksys_apll12_div_out1_pdn",
		"cksys_apll12_div_out2_pdn",
		"cksys_apll12_div_out3_pdn",
		"cksys_apll12_div_out4_pdn",
		"cksys_apll12_div_out6_pdn",
		"cksys_apll12_div_fmi2s_p",
		"cksys_apll12_div_m_pdn",
		"cksys_apll12_div_b_pdn",

		/* infra_infracfg_ao_reg */
		"infra_ao_ccif1_ap",
		"infra_ao_ccif1_md",
		"infra_ao_ccif_ap",
		"infra_ao_ccif_md",
		"infra_ao_cldmabclk",
		"infra_ao_ccif5_md",
		"infra_ao_ccif2_ap",
		"infra_ao_ccif2_md",
		"infra_ao_dpmaif_main",
		"infra_ao_ccif4_md",
		"infra_ao_dpmaif_26m_set",

		/* apmixedsys */
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"emipll",
		"apll1",
		"apll2",
		"tvdpll",

		/* pericfg_ao_reg */
		"peri_ao_uart0",
		"peri_ao_uart1",
		"peri_ao_uart2",
		"peri_ao_uart3",
		"peri_ao_uart4",
		"peri_ao_uart5",
		"peri_ao_pwm_h",
		"peri_ao_pwm_b",
		"peri_ao_disp_pwm0",
		"peri_ao_disp_pwm1",
		"peri_ao_spi0_b",
		"peri_ao_spi1_b",
		"peri_ao_spi2_b",
		"peri_ao_spi3_b",
		"peri_ao_spi4_b",
		"peri_ao_spi5_b",
		"peri_ao_spi6_b",
		"peri_ao_spi7_b",
		"peri_ao_dma_b",
		"peri_ao_msdc1",
		"peri_ao_msdc1_div",
		"peri_ao_msdc1_mst_f",
		"peri_ao_msdc1_slv_h",
		"peri_ao_audio0",
		"peri_ao_audio1",
		"peri_ao_audio2",

		/* afe */
		"afe_aud_pad_mosi",
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
		"afe_cm2",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_fm_i2s",
		"afe_mtkaifv4",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in6",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
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
		"afe_etdm_in_dma0",
		"afe_etdm6_padtop",
		"afe_etdm7_padtop",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",
		"ufsao_unipro_sap_cfg",
		"ufsao_ufs_phy_ahb_s_bus",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",
		"ufspdn_ufshci_ufs_ahb",
		"ufspdn_ufshci_ufs_axi",

		/* imp_iic_top_wrap_s */
		"imp_iic_wrap_s_i2c2",
		"imp_iic_wrap_s_i2c4",
		"imp_iic_wrap_s_i2c7",
		"imp_iic_wrap_s_i2c8",
		"imp_iic_wrap_s_i2c9",
		"imp_iic_wrap_s_i2c10",
		"imp_iic_wrap_s_i2c11",
		"imp_iic_wrap_s_i2c12",

		/* imp_iic_top_wrap_w */
		"imp_iic_wrap_w_i2c0",
		"imp_iic_wrap_w_i2c1",
		"imp_iic_wrap_w_i2c3",
		"imp_iic_wrap_w_i2c5",
		"imp_iic_wrap_w_i2c6",

		/* mipi_csi_top_ctrl_0 */
		"mipi_csi_ck0_en",
		"mipi_csi_ck1_en",
		"mipi_csi_ck2_en",
		"mipi_csi_ck3_en",

		/* dispsys_config */
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl2_2l",
		"mm_disp_ovl3_2l",
		"mm_disp_rsz1",
		"mm_disp_rsz0",
		"mm_disp_tdshp0",
		"mm_disp_c3d0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_dither0",
		"mm_disp_tdshp1",
		"mm_disp_c3d1",
		"mm_disp_ccorr2",
		"mm_disp_ccorr3",
		"mm_disp_gamma1",
		"mm_disp_dither1",
		"mm_disp_splitter0",
		"mm_disp_dsc_wrap0",
		"mm_CLK0",
		"mm_CLK1",
		"mm_disp_wdma1",
		"mm_disp_apb_bus",
		"mm_disp_fake_eng0",
		"mm_disp_fake_eng1",
		"mm_disp_mutex0",
		"mm_smi_common",
		"mm_dsi0_ck",
		"mm_dsi1_ck",
		"mm_26m_ck",

		/* imgsys_main */
		"img_fdvt",
		"img_larb12",
		"img_odpm26",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common3",
		"img_sub_common4",
		"img26",

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
		"wpe_tnr_dip1_dwpe",
		"wpe_tnr_dip1_me",
		"wpe_tnr_dip1_mmg",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",

		/* traw_cap_dip1 */
		"traw__dip1_cap",

		/* img_vcore_d1a */
		"img_vcore_sub0",
		"img_vcore_sub1",
		"img_vcore_img_26m",

		/* vdec_gcon_base */
		"vde2_larb1_cken",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_larb",
		"ven1_venc",
		"ven1_jpgenc",
		"ven1_gals",

		/* vlp_cksys_top */
		"vlp_scp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_p_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_dbgao_26m_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_sspm_f26m_sel",
		"vlp_srck_sel",
		"vlp_sramrc_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_spi_hs_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_sspm_ulposc_sel",
		"vlp_tia_ulposc_sel",
		"vlp_apxgpt_26m_sel",

		/* cam_main_r1a */
		"cam_m_cam_main",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_subc",
		"cam_m_cam_seninf_tg_suba",
		"cam_m_cam_seninf_tg_subb",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_sub_comm_0c_0",
		"cam_m_sub_comm_1",
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
		"cam_mr_fake_eng",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
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
		"cam_rb_cam_26m",

		/* camsys_rmsb */
		"camsys_rmsb_larbx",
		"camsys_rmsb_cam",
		"camsys_rmsb_camtg",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* cam_vcore_r1a */
		"_vcore",
		"_26m",
		"_bls_part",
		"_bls_full",
		"_resv0_GCON_0",
		"_resv1_GCON_0",
		"_vcore_cam2mm0",
		"_vcore_cam2mm1",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
		"mminfra_smi",
		"mminfra_gce_m2",
		"mminfra_gce_26m",

		/* mdpsys_config */
		"mdp_mutex0",
		"mdp_apb_bus",
		"mdp_smi0",
		"mdp_rdma0",
		"mdp_fg0",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_color0",
		"mdp_wrot0",
		"mdp_fake_eng0",
		"mdp_dli_async0",
		"mdp_dli_async1",
		"mdp_rdma1",
		"mdp_fg1",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_color1",
		"mdp_wrot1",
		"mdp_rsz2",
		"mdp_wrot2",
		"mdp_dlo_async0",
		"mdp_rsz3",
		"mdp_wrot3",
		"mdp_dlo_async1",
		"mdp_hre_mdpsys",
		"mdp_fmm_img_dl_async0",
		"mdp_fmm_img_dl_async1",
		"mdp_fimg_img_dl_async0",
		"mdp_fimg_img_dl_async1",


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
	struct device *tmp_dev;
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
				case TEST_TYPE_GENPD:
					tmp_dev = TEST_CLK_TO_GENPD(t_clks[i]);
					ret = pm_runtime_get_sync(tmp_dev);
					if (ret < 0) {
						pr_notice("%s fail to power on(%d)\n",
							dev_name(tmp_dev),
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
				case TEST_TYPE_GENPD:
					tmp_dev = TEST_CLK_TO_GENPD(t_clks[i]);
					ret = pm_runtime_put_sync(tmp_dev);
					if (ret < 0) {
						pr_notice("%s fail to power off(%d)\n",
							dev_name(tmp_dev),
							ret);
						goto ERR;
					}
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
					clk_set_parent(tmp_clk, tmp_clk_p);
					goto ERR;
				}
			}
			/* reset to original parent */
			clk_set_parent(tmp_clk, tmp_clk_p);
			break;
		default:
			pr_notice("unknown test type\n");
			goto ERR;
		}
		thread_cnt++;

		if (test_clk->repeat_time != -1 &&
		    (unsigned int)test_clk->repeat_time == thread_cnt)
			break;

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
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6881_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6881_all_clk_names,
	.start_task = start_clkdbg_test_task,
};

static int clk_dbg_mt6881_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6881_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6881_drv = {
	.probe = clk_dbg_mt6881_probe,
	.driver = {
		.name = "clk-dbg-mt6881",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6881_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6881_drv, "clk-dbg-mt6881");
}

static void __exit clkdbg_mt6881_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6881_drv);
}

subsys_initcall(clkdbg_mt6881_init);
module_exit(clkdbg_mt6881_exit);
MODULE_LICENSE("GPL");
