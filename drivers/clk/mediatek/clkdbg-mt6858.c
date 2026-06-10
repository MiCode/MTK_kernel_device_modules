// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
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
#include "clkchk-mt6858.h"
#include "clk-fmeter.h"

#define THREAD_LEN      (16)
#define THREAD_NUM      (6)
#define PD_NUM          (3)

static int clkdbg_thread_cnt;

const char * const *get_mt6858_all_clk_names(void)
{
	static const char * const clks[] = {
		/* cksys_reg */
		"top_axi_sel",
		"top_axi_p_sel",
		"top_axi_ufs_sel",
		"top_bus_aximem_sel",
		"top_disp0_sel",
		"top_mminfra_sel",
		"top_mmup_sel",
		"top_uart_sel",
		"top_spi0_sel",
		"top_spi1_sel",
		"top_spi2_sel",
		"top_spi3_sel",
		"top_spi4_sel",
		"top_spi5_sel",
		"top_spi6_sel",
		"top_spi7_sel",
		"top_msdc_macro_1p_sel",
		"top_msdc30_1_sel",
		"top_msdc30_1_h_sel",
		"top_aud_intbus_sel",
		"top_atb_sel",
		"top_disp_pwm_sel",
		"top_usb_sel",
		"top_usb_xhci_sel",
		"top_i2c_sel",
		"top_seninf_sel",
		"top_seninf1_sel",
		"top_seninf2_sel",
		"top_aud_engen1_sel",
		"top_aud_engen2_sel",
		"top_aes_ufsfde_sel",
		"top_ufs_sel",
		"top_aud_1_sel",
		"top_aud_2_sel",
		"top_dpmaif_main_sel",
		"top_venc_sel",
		"top_vdec_sel",
		"top_pwm_sel",
		"top_audio_h_sel",
		"top_mcupm_sel",
		"top_mem_sub_sel",
		"top_mem_sub_p_sel",
		"top_mem_sub_ufs_sel",
		"top_emi_n_sel",
		"top_dsi_occ_sel",
		"top_ap2conn_host_sel",
		"top_img1_sel",
		"top_ipe_sel",
		"top_cam_sel",
		"top_camtm_sel",
		"top_md_emi_sel",
		"top_mfg_ref_sel",
		"top_mfgsc_ref_sel",
		"top_efuse_sel",
		"top_unipll_ses_sel",
		"top_dramulp_sel",
		"top_usb_frmcnt_sel",
		"top_apll_i2sin1_m_sel",
		"top_apll_i2sin2_m_sel",

		/* cksys_reg */
		"top_apll12_div_i2sin1",
		"top_apll12_div_i2sin2",

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
		"armpll-ll",
		"armpll-bl",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"tvdpll",
		"emipll",
		"adsppll",
		"apll1",
		"apll2",

		/* pericfg_ao */
		"perao_p_uart0",
		"perao_p_uart1",
		"perao_p_uart2",
		"perao_p_uart3",
		"perao_p_pwm_h",
		"perao_p_pwm_b",
		"perao_p_pwm_fb1",
		"perao_p_pwm_fb2",
		"perao_p_pwm_fb3",
		"perao_p_pwm_fb4",
		"perao_p_disp_pwm0",
		"perao_p_disp_pwm1",
		"perao_p_spi0_b",
		"perao_p_spi1_b",
		"perao_p_spi2_b",
		"perao_p_spi3_b",
		"perao_p_spi4_b",
		"perao_p_spi5_b",
		"perao_p_spi6_b",
		"perao_p_spi7_b",
		"perao_p_dma_b",
		"perao_p_msdc1",
		"perao_p_msdc1_h",
		"perao_p_msdc1_mst_f",
		"perao_p_msdc1_slv_h",
		"perao_p_audio0",
		"perao_p_audio1",
		"perao_p_audio2",

		/* afe */
		"afe_aud_pad_mosi",
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
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in4",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_out4",
		"afe_etdm_out2",
		"afe_etdm_out1",
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
		"impc_i2c1",
		"impc_i2c3",
		"impc_i2c5",
		"impc_i2c6",
		"impc_i2c10",
		"impc_i2c12",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_sys",
		"ufsao_unipro_sap_cfg",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",
		"ufspdn_ufshci_ufs_ahb",
		"ufspdn_ufs_26m_ck",

		/* imp_iic_wrap_es */
		"impes_i2c4",
		"impes_i2c7",
		"impes_i2c8",

		/* imp_iic_wrap_s */
		"imps_i2c0",
		"imps_i2c2",
		"imps_i2c9",
		"imps_i2c11",

		/* dispsys_config */
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl2_2l",
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
		"mm_disp_dsc_wrap0",
		"mm_CLK0",
		"mm_disp_wdma1",
		"mm_disp_apb_bus",
		"mm_disp_fake_eng0",
		"mm_disp_fake_eng1",
		"mm_disp_mutex0",
		"mm_smi_common",
		"mm_dsi0_ck",
		"mm_26m_ck",

		/* imgsys1 */
		"imgsys1_larb9",
		"imgsys1_larb10",
		"imgsys1_dip",

		/* imgsys2 */
		"imgsys2_larb9",
		"imgsys2_mfb",
		"imgsys2_wpe",
		"imgsys2_mss",

		/* vdec_gcon_base */
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
		"ven1_gals",

		/* vlp_cksys_top */
		"vlp_scp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_p_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_sspm_f26m_sel",
		"vlp_srck_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_sspm_ulposc_sel",
		"vlp_tia_ulposc_sel",
		"vlp_apxgpt_26m_sel",
		"vlp_camtg0_sel",
		"vlp_camtg1_sel",
		"vlp_camtg2_sel",
		"vlp_camtg3_sel",
		"vlp_ssr_pka_sel",
		"vlp_ssr_dma_sel",
		"vlp_ssr_kdf_sel",
		"vlp_ssr_rng_sel",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_cam",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv1",
		"cam_m_camsv2",
		"cam_m_camsv3",
		"cam_m_cam2mm_gals",
		"cam_m_pda",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* ipesys */
		"ipe_larb20",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_rsc",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
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
		"mdp_rsz2",
		"mdp_wrot2",
		"mdp_dlo_async0",
		"mdp_fmm_img_dl_async0",
		"mdp_fmm_img_dl_async1",
		"mdp_fimg_img_dl_async0",
		"mdp_fimg_img_dl_async1",
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

static struct clkdbg_ops clkdbg_mt6858_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6858_all_clk_names,
	.start_task = start_clkdbg_test_task,
};

static int clk_dbg_mt6858_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6858_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6858_drv = {
	.probe = clk_dbg_mt6858_probe,
	.driver = {
		.name = "clk-dbg-mt6858",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6858_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6858_drv, "clk-dbg-mt6858");
}

static void __exit clkdbg_mt6858_exit(void)
{
	unset_clkdbg_ops();
	platform_driver_unregister(&clk_dbg_mt6858_drv);
}

subsys_initcall(clkdbg_mt6858_init);
module_exit(clkdbg_mt6858_exit);
MODULE_LICENSE("GPL");
