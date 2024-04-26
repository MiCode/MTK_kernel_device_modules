// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/clk-provider.h>
#include <linux/io.h>

#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>


#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, h, l, v)	\
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "hd_faxi_ck"),
	FMCLK(CKGEN,  2, "hf_fscp_ck"),
	FMCLK(CKGEN,  3, "hf_fmfg_ck"),
	FMCLK(CKGEN,  4, "f_fcamtg_ck"),
	FMCLK(CKGEN,  5, "f_fcamtg1_ck"),
	FMCLK(CKGEN,  6, "f_fcamtg2_ck"),
	FMCLK(CKGEN,  7, "f_fcamtg3_ck"),
	FMCLK(CKGEN,  8, "f_fcamtg4_ck"),
	FMCLK(CKGEN,  9, "f_fcamtg5_ck"),
	FMCLK(CKGEN,  10, "f_fcamtg6_ck"),
	FMCLK(CKGEN,  11, "f_fuart_ck"),
	FMCLK(CKGEN,  12, "hf_fspi_ck"),
	FMCLK(CKGEN,  13, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN,  14, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN,  15, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN,  16, "hf_faudio_ck"),
	FMCLK(CKGEN,  17, "hf_faud_intbus_ck"),
	FMCLK(CKGEN,  18, "hf_faud_1_ck"),
	FMCLK(CKGEN,  19, "hf_faud_2_ck"),
	FMCLK(CKGEN,  20, "hf_faud_engen1_ck"),
	FMCLK(CKGEN,  21, "hf_faud_engen2_ck"),
	FMCLK(CKGEN,  22, "f_fdisp_pwm_ck"),
	FMCLK(CKGEN,  23, "hf_sspm_ck"),
	FMCLK(CKGEN,  24, "hf_fdxcc_ck"),
	FMCLK(CKGEN,  25, "hf_fusb_top_ck"),
	FMCLK(CKGEN,  26, "hf_fsrck_ck"),
	FMCLK(CKGEN,  27, "hf_fspm_ck"),
	FMCLK(CKGEN,  28, "hf_fi2c_ck"),
	FMCLK(CKGEN,  29, "f_fpwm_ck"),
	FMCLK(CKGEN,  30, "f_fseninf_ck"),
	FMCLK(CKGEN,  31, "f_fseninf1_ck"),
	FMCLK(CKGEN,  32, "f_fseninf2_ck"),
	FMCLK(CKGEN,  33, "f_fseninf3_ck"),
	FMCLK(CKGEN,  34, "hdf_faes_msdcfde_ck"),
	FMCLK(CKGEN,  35, "f_fpwrap_ulposc_ck"),
	FMCLK(CKGEN,  36, "f_fcamtm_ck"),
	FMCLK(CKGEN,  37, "hf_fvenc_ck"),
	FMCLK(CKGEN,  38, "hf_fcam_ck"),
	FMCLK(CKGEN,  39, "hf_fimg1_ck"),
	FMCLK(CKGEN,  40, "hf_fipe_ck"),
	FMCLK(CKGEN,  41, "hf_dpmaif_ck"),
	FMCLK(CKGEN,  42, "hf_fvdec_ck"),
	FMCLK(CKGEN,  43, "hf_fdisp_ck"),
	FMCLK(CKGEN,  44, "hf_fmdp_ck"),
	FMCLK(CKGEN,  45, "hf_faudio_h_ck"),
	FMCLK(CKGEN,  46, "hf_fufs_ck"),
	FMCLK(CKGEN,  47, "hf_faes_fde_ck"),
	FMCLK(CKGEN,  48, "hf_audiodsp_ck"),
	FMCLK(CKGEN,  49, "hg_fdvfsrc_ck"),
	FMCLK(CKGEN,  50, "hg_fdvfsrc_ck"),
	FMCLK(CKGEN,  51, "hf_dsi_occ_ck"),
	FMCLK(CKGEN,  52, "hf_fspmi_mst_ck"),
	FMCLK(ABIST,  7, "AD_CCIPLL_CK"),
	FMCLK(ABIST,  8, "AD_ARMPLL_L_CK"),
	FMCLK(ABIST,  9, "AD_ARMPLL_CK"),
	FMCLK(ABIST,  10, "AD_PLLGP1_TST_CK"),
	FMCLK(ABIST,  11, "AD_MDBPIPLL_CK"),
	FMCLK(ABIST,  12, "AD_MDBRPPLL_CK"),
	FMCLK(ABIST,  13, "AD_MDVDSPPLL_CK"),
	FMCLK(ABIST,  14, "AD_MDMCUPLL_CK"),
	FMCLK(ABIST,  15, "AD_APLL2_CK"),
	FMCLK(ABIST,  16, "AD_APLL1_CK"),
	FMCLK(ABIST,  17, "AD_USB20_192M_CK"),
	FMCLK(ABIST,  19, "AD_CCIPLL_CK"),
	FMCLK(ABIST,  20, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST,  21, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST,  24, "AD_MAINPLL_CK"),
	FMCLK(ABIST,  25, "AD_MDPLL1_FS26M_CK_guide"),
	FMCLK(ABIST,  26, "AD_MFGPLL_CK"),
	FMCLK(ABIST,  27, "AD_MMPLL_CK"),
	FMCLK(ABIST,  28, "AD_ADSPPLL_CK"),
	FMCLK(ABIST,  29, "AD_MPLL_208M_CK"),
	FMCLK(ABIST,  30, "AD_MSDCPLL_CK"),
	FMCLK(ABIST,  34, "AD_ULPOSC2_CK"),
	FMCLK(ABIST,  35, "AD_ULPOSC_CK"),
	FMCLK(ABIST,  36, "AD_UNIVPLL_CK"),
	FMCLK(ABIST,  40, "ad_wbg_dig_bpll_ck"),
	FMCLK(ABIST,  41, "UFS_MP_CLK2FREQ"),
	FMCLK(ABIST,  42, "AD_RCLRPLL_DIV4_CK"),
	FMCLK(ABIST,  43, "AD_RPHYPLL_DIV4_CK"),
	FMCLK(ABIST,  44, "fmem_ck_aft_dsm_ch0"),
	FMCLK(ABIST,  45, "fmem_ck_aft_dsm_ch1"),
	FMCLK(ABIST,  46, "fmem_ck_bfe_dcm_ch0"),
	FMCLK(ABIST,  47, "fmem_ck_bfe_dcm_ch1"),
	FMCLK(ABIST,  48, "mcusys_arm_clk_out_all"),
	{}
};

#define _CKGEN(x)		(rb[topckgen].virt + (x))
#define CLK_CFG_0		_CKGEN(0x40)
#define CLK_CFG_1		_CKGEN(0x50)
#define CLK_CFG_2		_CKGEN(0x60)
#define CLK_CFG_3		_CKGEN(0x70)
#define CLK_CFG_4		_CKGEN(0x80)
#define CLK_CFG_5		_CKGEN(0x90)
#define CLK_CFG_6		_CKGEN(0xa0)
#define CLK_CFG_7		_CKGEN(0xb0)
#define CLK_CFG_8		_CKGEN(0xc0)
#define CLK_CFG_9		_CKGEN(0xd0)
#define CLK_CFG_10		_CKGEN(0xe0)
#define CLK_CFG_11		_CKGEN(0xf0)
#define CLK_CFG_12		_CKGEN(0x100)
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

#define _SCPSYS(x)		(rb[scpsys].virt + (x))
#define SPM_PWR_STATUS		_SCPSYS(0x16C)
#define SPM_PWR_STATUS_2ND	_SCPSYS(0x170)


#define PLL_HP_CON0			(rb[apmixed].virt + 0x014)
#define PLL_TEST_CON1			(rb[apmixed].virt + 0x064)
#define TEST_DBG_CTRL			(rb[topckgen].virt + 0x38)
#define FREQ_MTR_CTRL_REG		(rb[topckgen].virt + 0x10)
#define FREQ_MTR_CTRL_RDATA		(rb[topckgen].virt + 0x14)

#define RG_FQMTR_CKDIV_GET(x)		(((x) >> 28) & 0x3)
#define RG_FQMTR_CKDIV_SET(x)		(((x) & 0x3) << 28)
#define RG_FQMTR_FIXCLK_SEL_GET(x)	(((x) >> 24) & 0x3)
#define RG_FQMTR_FIXCLK_SEL_SET(x)	(((x) & 0x3) << 24)
#define RG_FQMTR_MONCLK_SEL_GET(x)	(((x) >> 16) & 0x7f)
#define RG_FQMTR_MONCLK_SEL_SET(x)	(((x) & 0x7f) << 16)
#define RG_FQMTR_MONCLK_EN_GET(x)	(((x) >> 15) & 0x1)
#define RG_FQMTR_MONCLK_EN_SET(x)	(((x) & 0x1) << 15)
#define RG_FQMTR_MONCLK_RST_GET(x)	(((x) >> 14) & 0x1)
#define RG_FQMTR_MONCLK_RST_SET(x)	(((x) & 0x1) << 14)
#define RG_FQMTR_MONCLK_WINDOW_GET(x)	(((x) >> 0) & 0xfff)
#define RG_FQMTR_MONCLK_WINDOW_SET(x)	(((x) & 0xfff) << 0)

#define RG_FQMTR_CKDIV_DIV_2		0
#define RG_FQMTR_CKDIV_DIV_4		1
#define RG_FQMTR_CKDIV_DIV_8		2
#define RG_FQMTR_CKDIV_DIV_16		3

#define RG_FQMTR_FIXCLK_26MHZ		0
#define RG_FQMTR_FIXCLK_32KHZ		2

#define RG_FQMTR_EN     1
#define RG_FQMTR_RST    1

#define RG_FRMTR_WINDOW     519

/*
 * clkdbg dump_state
 */

static const char * const *get_mt6781_all_clk_names(void)
{
	static const char * const clks[] = {
		/* APMIXEDSYS */
		"mainpll",
		"univ2pll",
		"msdcpll",
		"mfgpll",
		"mmpll",
		"tvdpll",
		"apll1",
		"apll2",
		"adsppll",

		/* TOP */
		"axi_sel",
		"mm_sel",
		"scp_sel",

		"img_sel",
		"ipe_sel",
		"dpe_sel",
		"cam_sel",

		"ccu_sel",
		"dsp_sel",
		"dsp1_sel",
		"dsp2_sel",

		"dsp3_sel",
		"ipu_if_sel",
		"mfg_sel",
		"f52m_mfg_sel",

		"camtg_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",

		"uart_sel",
		"spi_sel",
		"msdc50_hclk_sel",
		"msdc50_0_sel",

		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"fpwrap_ulposc_sel",

		"atb_sel",
		"sspm_sel",
		"dpi0_sel",
		"scam_sel",

		"disppwm_sel",
		"usb_top_sel",
		"ssusb_top_xhci_sel",
		"spm_sel",

		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",

		"dxcc_sel",
		"aud_eng1_sel",
		"aud_eng2_sel",
		"faes_ufsfde_sel",

		"fufs_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",

		"dpmaif_parents",
		"venc_sel",
		"vdec_sel",
		"camtm_sel",

		"pwm_sel",
		"audio_h_sel",
		"camtg5_sel",


		/* INFRACFG */
		"infra_pmic_tmr",
		"infra_pmic_ap",
		"infra_pmic_md",
		"infra_pmic_conn",
		"infra_scp",
		"infra_sej",
		"infra_apxgpt",
		"infra_icusb",
		"infra_gce",
		"infra_therm",
		"infra_i2c0",
		"infra_i2c1",
		"infra_i2c2",
		"infra_i2c3",
		"infra_pwm_hclk",
		"infra_pwm1",
		"infra_pwm2",
		"infra_pwm3",
		"infra_pwm4",
		"infra_pwm",
		"infra_uart0",
		"infra_uart1",
		"infra_uart2",
		"infra_uart3",
		"infra_gce_26m",
		"infra_cqdma_fpc",
		"infra_btif",

		"infra_spi0",
		"infra_msdc0",
		"infra_msdc1",
		"infra_msdc2",
		"infra_msdc0_sck",
		"infra_dvfsrc",
		"infra_gcpu",
		"infra_trng",
		"infra_auxadc",
		"infra_cpum",
		"infra_ccif1_ap",
		"infra_ccif1_md",
		"infra_auxadc_md",
		"infra_msdc1_sck",
		"infra_msdc2_sck",
		"infra_apdma",
		"infra_xiu",
		"infra_device_apc",
		"infra_ccif_ap",
		"infra_debugsys",
		"infra_audio",
		"infra_ccif_md",
		"infra_dxcc_sec_core",
		"infra_dxcc_ao",
		"infra_dramc_f26m",

		"infra_irtx",
		"infra_disppwm",
		"infra_cldma_bclk",
		"infracfg_ao_audio_26m_bclk_ck",
		"infra_spi1",
		"infra_i2c4",
		"infra_md_tmp_share",
		"infra_spi2",
		"infra_spi3",
		"infra_unipro_sck",
		"infra_unipro_tick",
		"infra_ufs_mp_sap_bck",
		"infra_md32_bclk",
		"infra_sspm",
		"infra_unipro_mbist",
		"infra_sspm_bus_hclk",
		"infra_i2c5",
		"infra_i2c5_arbiter",
		"infra_i2c5_imm",
		"infra_i2c1_arbiter",
		"infra_i2c1_imm",
		"infra_i2c2_arbiter",
		"infra_i2c2_imm",
		"infra_spi4",
		"infra_spi5",
		"infra_cqdma",
		"infra_ufs",
		"infra_aes",
		"infra_ufs_tick",
		"infra_ssusb_xhci",

		"infra_msdc0_self",
		"infra_msdc1_self",
		"infra_msdc2_self",
		"infra_sspm_26m_self",
		"infra_sspm_32k_self",
		"infra_ufs_axi",
		"infra_i2c6",
		"infra_ap_msdc0",
		"infra_md_msdc0",
		"infra_ccif2_ap",
		"infra_ccif2_md",
		"infra_ccif3_ap",
		"infra_ccif3_md",
		"infra_sej_f13m",
		"infra_aes_bclk",
		"infra_i2c7",
		"infra_i2c8",
		"infra_fbist2fpc",
		"infra_dpmaif",
		"infra_fadsp",
		"infra_ccif4_ap",
		"infra_ccif4_md",
		"infra_spi6",
		"infra_spi7",

		/* AUDIO */
		"aud_afe",
		"aud_22m",
		"aud_24m",
		"aud_apll2_tuner",
		"aud_apll_tuner",
		"aud_tdm",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_nle",
		"aud_i2s1_bclk",
		"aud_i2s2_bclk",
		"aud_i2s3_bclk",
		"aud_i2s4_bclk",
		"aud_i2s5_bclk",
		"aud_conn_i2s",
		"aud_general1",
		"aud_general2",
		"aud_dac_hires",
		"aud_adc_hires",
		"aud_adc_hires_tml",
		"aud_pdn_adda6_adc",
		"aud_adda6_adc_hires",
		"aud_3rd_dac",
		"aud_3rd_dac_predis",
		"aud_3rd_dac_tml",
		"aud_3rd_dac_hires",

		/* CAM */
		"camsys_larb10",
		"camsys_dfp_vad",
		"camsys_larb11",
		"camsys_larb9",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_camsv2",
		"camsys_camsv3",
		"camsys_ccu",
		"camsys_fake_eng",

		/* IMG */
		"imgsys_larb5",
		"imgsys_larb6",
		"imgsys_dip",
		"imgsys_mfb",
		"imgsys_wpe_a",
		/* IPE */
		"ipe_larb7",
		"ipe_larb8",
		"ipe_smi_subcom",
		"ipe_fd",
		"ipe_fe",
		"ipe_rsc",
		"ipe_dpe",

		/* MFG */
		"mfg_cfg_bg3d",

		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
		"mm_mdp_dl_txck",
		"mm_ipu_dl_txck",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wrot0",
		"mm_mdp_wrot1",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_disp_split",
		/* MM1 */
		"mm_dsi0_mmck",
		"mm_dsi0_ifck",
		"mm_dpi_mmck",
		"mm_dpi_ifck",
		"mm_fake_eng2",
		"mm_mdp_dl_rxck",
		"mm_ipu_dl_rxck",
		"mm_26m",
		"mm_mmsys_r2y",
		"mm_disp_rsz",
		"mm_mdp_aal",
		"mm_mdp_hdr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
		"mm_disp_pm0",
		"mm_disp_hrt_bw",
		"mm_disp_ovl_fbdc",

		/* VDEC */
		"vdec_cken",
		/* VDEC1 */
		"vdec_larb1_cken",

		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
		"venc_gals",

		/* APUSYS CONN */
		"apu_conn_apu",
		"apu_conn_ahb",
		"apu_conn_axi",
		"apu_conn_isp",
		"apu_conn_cam_adl",
		"apu_conn_img_adl",
		"apu_conn_emi_26m",
		"apu_conn_vpu_udi",

		/* APUSYS APU0 */
		"apu0_apu",
		"apu0_axi",
		"apu0_jtag",

		/* APUSYS APU1 */
		"apu1_apu",
		"apu1_axi",
		"apu1_jtag",

		/* APUSYS VCORE */
		"apu_vcore_ahb",
		"apu_vcore_axi",
		"apu_vcore_adl",
		"apu_vcore_qos",

		/* APUSYS MDLA */
		"mdla_b0",
		"mdla_b1",
		"mdla_b2",
		"mdla_b3",
		"mdla_b4",
		"mdla_b5",
		"mdla_b6",
		"mdla_b7",
		"mdla_b8",
		"mdla_b9",
		"mdla_b10",
		"mdla_b11",
		"mdla_b12",
		"mdla_apb",

		/* SCPSYS */
		"pg_md1",
		"pg_conn",
		"pg_dis",
		"pg_cam",
		"pg_isp",
		"pg_ipe",
		"pg_ven",
		"pg_vde",
		"pg_audio",
		"pg_mfg0",
		"pg_mfg1",
		"pg_mfg2",
		"pg_mfg3",
		"pg_mfg4",
		"pg_vpu_vcore_d",
		"pg_vpu_vcore_s",
		"pg_vpu_conn_d",
		"pg_vpu_conn_s",
		"pg_vpu_core0_d",
		"pg_vpu_core0_s",
		"pg_vpu_core1_d",
		"pg_vpu_core1_s",
		"pg_vpu_core2_d",
		"pg_vpu_core2_s",
		/* end */
		NULL
	};

	return clks;
}

#include <linux/seq_file.h>

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

static struct clkdbg_ops clkdbg_mt6781_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6781_all_clk_names,
};

static int clk_dbg_mt6781_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6781_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6781_drv = {
	.probe = clk_dbg_mt6781_probe,
	.driver = {
		.name = "clk-dbg-mt6781",
		.owner = THIS_MODULE,
	},
};

static int __init clkdbg_mt6781_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6781_drv, "clk-dbg-mt6781");

}


static void __exit clkdbg_mt6781_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6781_drv);
}

subsys_initcall(clkdbg_mt6781_init);
module_exit(clkdbg_mt6781_exit);
MODULE_LICENSE("GPL");
