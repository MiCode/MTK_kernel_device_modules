// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6881-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6881.h"
#include "clk-fmeter.h"
#include "clk-mt6881-fmeter.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0
#define EVT_LEN				40
#define PWR_ID_SHIFT			0
#define PWR_STA_SHIFT			8
#define HWV_INT_MTCMOS_TRIGGER		0x0008
#define HWV_IRQ_STATUS			0x0500 //FIXME

static DEFINE_SPINLOCK(pwr_trace_lock);
static unsigned int pwr_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

static void trace_power_event(unsigned int id, unsigned int pwr_sta)
{
	unsigned long flags = 0;

	if (id >= MT6881_CHK_PD_NUM)
		return;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pwr_event[evt_cnt] = (id << PWR_ID_SHIFT) | (pwr_sta << PWR_STA_SHIFT);
	evt_cnt++;
	if (evt_cnt >= EVT_LEN)
		evt_cnt = 0;

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

static void dump_power_event(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pr_notice("first idx: %d\n", evt_cnt);
	for (i = 0; i < EVT_LEN; i += 5)
		pr_notice("pwr_evt[%d] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				i,
				pwr_event[i],
				pwr_event[i + 1],
				pwr_event[i + 2],
				pwr_event[i + 3],
				pwr_event[i + 4]);

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

/*
 * The clk names in Mediatek CCF.
 */

/* afe */
struct pd_check_swcg afe_swcgs[] = {
	SWCG("afe_aud_pad_mosi"),
	SWCG("afe_dl1_dac_tml"),
	SWCG("afe_dl1_dac_hires"),
	SWCG("afe_dl1_dac"),
	SWCG("afe_dl1_predis"),
	SWCG("afe_dl1_nle"),
	SWCG("afe_dl0_dac_tml"),
	SWCG("afe_dl0_dac_hires"),
	SWCG("afe_dl0_dac"),
	SWCG("afe_dl0_predis"),
	SWCG("afe_dl0_nle"),
	SWCG("afe_pcm1"),
	SWCG("afe_pcm0"),
	SWCG("afe_cm2"),
	SWCG("afe_cm1"),
	SWCG("afe_cm0"),
	SWCG("afe_stf"),
	SWCG("afe_hw_gain23"),
	SWCG("afe_hw_gain01"),
	SWCG("afe_fm_i2s"),
	SWCG("afe_mtkaifv4"),
	SWCG("afe_ul1_aht"),
	SWCG("afe_ul1_adc_hires"),
	SWCG("afe_ul1_tml"),
	SWCG("afe_ul1_adc"),
	SWCG("afe_ul0_aht"),
	SWCG("afe_ul0_adc_hires"),
	SWCG("afe_ul0_tml"),
	SWCG("afe_ul0_adc"),
	SWCG("afe_etdm_in6"),
	SWCG("afe_etdm_in2"),
	SWCG("afe_etdm_in1"),
	SWCG("afe_etdm_in0"),
	SWCG("afe_etdm_out6"),
	SWCG("afe_etdm_out2"),
	SWCG("afe_etdm_out1"),
	SWCG("afe_etdm_out0"),
	SWCG("afe_general7_asrc"),
	SWCG("afe_general6_asrc"),
	SWCG("afe_general5_asrc"),
	SWCG("afe_general4_asrc"),
	SWCG("afe_general3_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general0_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_audio_hopping_ck"),
	SWCG("afe_audio_f26m_ck"),
	SWCG("afe_apll1_ck"),
	SWCG("afe_apll2_ck"),
	SWCG("afe_h208m_ck"),
	SWCG("afe_apll_tuner2"),
	SWCG("afe_apll_tuner1"),
	SWCG("afe_etdm_in_dma0"),
	SWCG("afe_etdm6_padtop"),
	SWCG("afe_etdm7_padtop"),
	SWCG(NULL),
};
/* mipi_csi_top_ctrl_0 */
struct pd_check_swcg mipi_csi_top_ctrl_0_swcgs[] = {
	SWCG("mipi_csi_ck0_en"),
	SWCG("mipi_csi_ck1_en"),
	SWCG("mipi_csi_ck2_en"),
	SWCG("mipi_csi_ck3_en"),
	SWCG(NULL),
};
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ovl2_2l"),
	SWCG("mm_disp_ovl3_2l"),
	SWCG("mm_disp_rsz1"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_tdshp0"),
	SWCG("mm_disp_c3d0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_tdshp1"),
	SWCG("mm_disp_c3d1"),
	SWCG("mm_disp_ccorr2"),
	SWCG("mm_disp_ccorr3"),
	SWCG("mm_disp_gamma1"),
	SWCG("mm_disp_dither1"),
	SWCG("mm_disp_splitter0"),
	SWCG("mm_disp_dsc_wrap0"),
	SWCG("mm_CLK0"),
	SWCG("mm_CLK1"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_disp_apb_bus"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_mutex0"),
	SWCG("mm_smi_common"),
	SWCG("mm_dsi0_ck"),
	SWCG("mm_dsi1_ck"),
	SWCG("mm_26m_ck"),
	SWCG(NULL),
};
/* imgsys_main */
struct pd_check_swcg imgsys_main_swcgs[] = {
	SWCG("img_fdvt"),
	SWCG("img_larb12"),
	SWCG("img_odpm26"),
	SWCG("img_larb9"),
	SWCG("img_traw0"),
	SWCG("img_traw1"),
	SWCG("img_dip0"),
	SWCG("img_wpe0"),
	SWCG("img_ipe"),
	SWCG("img_wpe1"),
	SWCG("img_wpe2"),
	SWCG("img_sub_common0"),
	SWCG("img_sub_common1"),
	SWCG("img_sub_common3"),
	SWCG("img_sub_common4"),
	SWCG("img26"),
	SWCG(NULL),
};
/* dip_top_dip1 */
struct pd_check_swcg dip_top_dip1_swcgs[] = {
	SWCG("dip_dip1_dip_top"),
	SWCG("dip_dip1_dip_gals0"),
	SWCG("dip_dip1_dip_gals1"),
	SWCG("dip_dip1_dip_gals2"),
	SWCG("dip_dip1_dip_gals3"),
	SWCG("dip_dip1_larb10"),
	SWCG("dip_dip1_larb15"),
	SWCG("dip_dip1_larb38"),
	SWCG("dip_dip1_larb39"),
	SWCG(NULL),
};
/* dip_nr1_dip1 */
struct pd_check_swcg dip_nr1_dip1_swcgs[] = {
	SWCG("dip_nr1_dip1_larb"),
	SWCG("dip_nr1_dip1_dip_nr1"),
	SWCG(NULL),
};
/* dip_nr2_dip1 */
struct pd_check_swcg dip_nr2_dip1_swcgs[] = {
	SWCG("dip_nr2_dip1_dip_nr"),
	SWCG("dip_nr2_dip1_larb15"),
	SWCG("dip_nr2_dip1_larb39"),
	SWCG(NULL),
};
/* wpe_eis_dip1 */
struct pd_check_swcg wpe_eis_dip1_swcgs[] = {
	SWCG("wpe_eis_dip1_larb_u0"),
	SWCG("wpe_eis_dip1_larb_u1"),
	SWCG("wpe_eis_dip1_gals_u0"),
	SWCG("wpe_eis_dip1_gals_u1"),
	SWCG("wpe_eis_dip1_wpe_macro"),
	SWCG("wpe_eis_dip1_wpe"),
	SWCG("wpe_eis_dip1_pqdip"),
	SWCG("wpe_eis_dip1_pqdip_dma"),
	SWCG("wpe_eis_dip1_omc"),
	SWCG("wpe_eis_dip1_dwpe"),
	SWCG("wpe_eis_dip1_me"),
	SWCG("wpe_eis_dip1_mmg"),
	SWCG("wpe_eis_dip1_wpe_26m"),
	SWCG(NULL),
};
/* wpe_tnr_dip1 */
struct pd_check_swcg wpe_tnr_dip1_swcgs[] = {
	SWCG("wpe_tnr_dip1_larb_u0"),
	SWCG("wpe_tnr_dip1_larb_u1"),
	SWCG("wpe_tnr_dip1_gals_u0"),
	SWCG("wpe_tnr_dip1_gals_u1"),
	SWCG("wpe_tnr_dip1_wpe_macro"),
	SWCG("wpe_tnr_dip1_wpe"),
	SWCG("wpe_tnr_dip1_pqdip"),
	SWCG("wpe_tnr_dip1_pqdip_dma"),
	SWCG("wpe_tnr_dip1_omc"),
	SWCG("wpe_tnr_dip1_dwpe"),
	SWCG("wpe_tnr_dip1_me"),
	SWCG("wpe_tnr_dip1_mmg"),
	SWCG(NULL),
};
/* traw_dip1 */
struct pd_check_swcg traw_dip1_swcgs[] = {
	SWCG("traw_dip1_larb28"),
	SWCG("traw_dip1_larb40"),
	SWCG("traw_dip1_traw"),
	SWCG(NULL),
};
/* traw_cap_dip1 */
struct pd_check_swcg traw_cap_dip1_swcgs[] = {
	SWCG("traw__dip1_cap"),
	SWCG(NULL),
};
/* img_vcore_d1a */
struct pd_check_swcg img_vcore_d1a_swcgs[] = {
	SWCG("img_vcore_sub0"),
	SWCG("img_vcore_sub1"),
	SWCG("img_vcore_img_26m"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb1_cken"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG("vde2_vdec_cken_eng"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_larb"),
	SWCG("ven1_venc"),
	SWCG("ven1_jpgenc"),
	SWCG("ven1_gals"),
	SWCG(NULL),
};
/* cam_main_r1a */
struct pd_check_swcg cam_main_r1a_swcgs[] = {
	SWCG("cam_m_cam_main"),
	SWCG("cam_m_cam_suba"),
	SWCG("cam_m_cam_subb"),
	SWCG("cam_m_cam_subc"),
	SWCG("cam_m_cam_seninf_tg_suba"),
	SWCG("cam_m_cam_seninf_tg_subb"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_sub_comm_0c_0"),
	SWCG("cam_m_sub_comm_1"),
	SWCG("cam_m_ips"),
	SWCG("cam_m_cam_asg"),
	SWCG("cam_m_cam_qof_con_1"),
	SWCG("cam_m_cam_bwr_con_1"),
	SWCG("cam_m_cam_rtcq_con_1"),
	SWCG("cam_m_cam_sdlcq_con_1"),
	SWCG("cam_m_cam_wla_con_1"),
	SWCG("cam_m_cam_dvc_con_1"),
	SWCG("cam_m_cam_cvfs_con_1"),
	SWCG(NULL),
};
/* camsys_mraw */
struct pd_check_swcg camsys_mraw_swcgs[] = {
	SWCG("cam_mr_larb13"),
	SWCG("cam_mr_larb14"),
	SWCG("cam_mr_larb19"),
	SWCG("cam_mr_larb25"),
	SWCG("cam_mr_larb26"),
	SWCG("cam_mr_larb29"),
	SWCG("cam_mr_seninf_camtm"),
	SWCG("cam_mr_camsv_top"),
	SWCG("cam_mr_camsv_a"),
	SWCG("cam_mr_camsv_b"),
	SWCG("cam_mr_camsv_c"),
	SWCG("cam_mr_camsv_d"),
	SWCG("cam_mr_camsv_e"),
	SWCG("cam_mr_camsv"),
	SWCG("cam_mr_pda0"),
	SWCG("cam_mr_pda1"),
	SWCG("cam_mr_fake_eng"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG("cam_ra_cam_26m"),
	SWCG(NULL),
};
/* camsys_rmsa */
struct pd_check_swcg camsys_rmsa_swcgs[] = {
	SWCG("camsys_rmsa_larbx"),
	SWCG("camsys_rmsa_cam"),
	SWCG("camsys_rmsa_camtg"),
	SWCG(NULL),
};
/* camsys_yuva */
struct pd_check_swcg camsys_yuva_swcgs[] = {
	SWCG("cam_ya_larbx"),
	SWCG("cam_ya_cam"),
	SWCG("cam_ya_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG("cam_rb_cam_26m"),
	SWCG(NULL),
};
/* camsys_rmsb */
struct pd_check_swcg camsys_rmsb_swcgs[] = {
	SWCG("camsys_rmsb_larbx"),
	SWCG("camsys_rmsb_cam"),
	SWCG("camsys_rmsb_camtg"),
	SWCG(NULL),
};
/* camsys_yuvb */
struct pd_check_swcg camsys_yuvb_swcgs[] = {
	SWCG("cam_yb_larbx"),
	SWCG("cam_yb_cam"),
	SWCG("cam_yb_camtg"),
	SWCG(NULL),
};
/* cam_vcore_r1a */
struct pd_check_swcg cam_vcore_r1a_swcgs[] = {
	SWCG("_vcore"),
	SWCG("_26m"),
	SWCG("_bls_part"),
	SWCG("_bls_full"),
	SWCG("_resv0_GCON_0"),
	SWCG("_resv1_GCON_0"),
	SWCG("_vcore_cam2mm0"),
	SWCG("_vcore_cam2mm1"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_mutex0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_smi0"),
	SWCG("mdp_rdma0"),
	SWCG("mdp_fg0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_color0"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_dli_async0"),
	SWCG("mdp_dli_async1"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_fg1"),
	SWCG("mdp_hdr1"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_color1"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rsz2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_dlo_async0"),
	SWCG("mdp_rsz3"),
	SWCG("mdp_wrot3"),
	SWCG("mdp_dlo_async1"),
	SWCG("mdp_hre_mdpsys"),
	SWCG("mdp_fmm_img_dl_async0"),
	SWCG("mdp_fmm_img_dl_async1"),
	SWCG("mdp_fimg_img_dl_async0"),
	SWCG("mdp_fimg_img_dl_async1"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	int pd_parent;			/* power domain parent id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6881_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6881_CHK_PD_CSI_RX, PD_NULL, mipi_csi_top_ctrl_0_swcgs, mipi_csi_top_ctrl_0},
	{MT6881_CHK_PD_DIS0, PD_NULL, dispsys_config_swcgs, mm},
	{MT6881_CHK_PD_ISP_MAIN, MT6881_CHK_PD_ISP_VCORE, imgsys_main_swcgs, img},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, dip_top_dip1_swcgs, dip_top_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, dip_nr1_dip1_swcgs, dip_nr1_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, dip_nr2_dip1_swcgs, dip_nr2_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, wpe_eis_dip1_swcgs, wpe_eis_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, wpe_tnr_dip1_swcgs, wpe_tnr_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, traw_dip1_swcgs, traw_dip1},
	{MT6881_CHK_PD_ISP_DIP1, MT6881_CHK_PD_ISP_MAIN, traw_cap_dip1_swcgs, traw_cap_dip1},
	{MT6881_CHK_PD_ISP_VCORE, MT6881_CHK_PD_MM_INFRA, img_vcore_d1a_swcgs, img_v},
	{MT6881_CHK_PD_VDE0, MT6881_CHK_PD_MM_INFRA, vdec_gcon_base_swcgs, vde2},
	{MT6881_CHK_PD_VEN0, MT6881_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6881_CHK_PD_CAM_MAIN, MT6881_CHK_PD_CAM_VCORE, cam_main_r1a_swcgs, cam_m},
	{MT6881_CHK_PD_CAM_MAIN, MT6881_CHK_PD_CAM_VCORE, camsys_mraw_swcgs, cam_mr},
	{MT6881_CHK_PD_CAM_SUBA, MT6881_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6881_CHK_PD_CAM_SUBA, MT6881_CHK_PD_CAM_MAIN, camsys_rmsa_swcgs, camsys_rmsa},
	{MT6881_CHK_PD_CAM_SUBA, MT6881_CHK_PD_CAM_MAIN, camsys_yuva_swcgs, cam_ya},
	{MT6881_CHK_PD_CAM_SUBB, MT6881_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6881_CHK_PD_CAM_SUBB, MT6881_CHK_PD_CAM_MAIN, camsys_rmsb_swcgs, camsys_rmsb},
	{MT6881_CHK_PD_CAM_SUBB, MT6881_CHK_PD_CAM_MAIN, camsys_yuvb_swcgs, cam_yb},
	{MT6881_CHK_PD_CAM_VCORE, MT6881_CHK_PD_MM_INFRA, cam_vcore_r1a_swcgs, cam_v},
	{MT6881_CHK_PD_DIS0, PD_NULL, mdpsys_config_swcgs, mdp},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6881_CHK_PD_NUM)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6881_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6881(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6881_CHK_PD_AUDIO,
	MT6881_CHK_PD_ISP_MAIN,
	MT6881_CHK_PD_ISP_DIP1,
	MT6881_CHK_PD_ISP_VCORE,
	MT6881_CHK_PD_VDE0,
	MT6881_CHK_PD_VEN0,
	MT6881_CHK_PD_CAM_MAIN,
	MT6881_CHK_PD_CAM_SUBA,
	MT6881_CHK_PD_CAM_SUBB,
	MT6881_CHK_PD_CAM_VCORE,
	MT6881_CHK_PD_DIS0,
	MT6881_CHK_PD_MM_INFRA,
	MT6881_CHK_PD_MM_PROC,
	MT6881_CHK_PD_CSI_RX,
	MT6881_CHK_PD_SSUSB,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6881_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static enum chk_sys_id debug_dump_id[] = { //FIXME
	spm,
	cksys_reg,
	infra_infracfg_ao_reg,
	apmixed,
	vlpcfg_reg_bus,
	//vlp_top,
	hwv,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	const struct fmeter_clk *fclks;
	int i, parent_id = PD_NULL;

	if (id >= MT6881_CHK_PD_NUM)
		return;

	fclks = mt_get_fmeter_clks();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	dump_power_event();

	set_subsys_reg_dump_mt6881(debug_dump_id);

	get_subsys_reg_dump_mt6881();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id) {
			print_subsys_reg_mt6881(mtk_subsys_check[i].chk_id);
			parent_id = mtk_subsys_check[i].pd_parent;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (parent_id == PD_NULL)
			break;

		if (mtk_subsys_check[i].pd_id == parent_id)
			print_subsys_reg_mt6881(mtk_subsys_check[i].chk_id);
	}

	mdelay(5000);
	BUG_ON(1);
}

static void external_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	dump_power_event();

	set_subsys_reg_dump_mt6881(debug_dump_id);
	get_subsys_reg_dump_mt6881();
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6881_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6881_CHK_PD_AUDIO, spm, 0x0E18, GENMASK(31, 30)},
	{MT6881_CHK_PD_ISP_MAIN, spm, 0x0E28, GENMASK(31, 30)},
	{MT6881_CHK_PD_ISP_DIP1, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6881_CHK_PD_ISP_VCORE, spm, 0x0E34, GENMASK(31, 30)},
	{MT6881_CHK_PD_VDE0, spm, 0x0E38, GENMASK(31, 30)},
	{MT6881_CHK_PD_VEN0, spm, 0x0E40, GENMASK(31, 30)},
	{MT6881_CHK_PD_CAM_MAIN, spm, 0x0E48, GENMASK(31, 30)},
	{MT6881_CHK_PD_CAM_SUBA, spm, 0x0E50, GENMASK(31, 30)},
	{MT6881_CHK_PD_CAM_SUBB, spm, 0x0E54, GENMASK(31, 30)},
	{MT6881_CHK_PD_CAM_VCORE, spm, 0x0E5C, GENMASK(31, 30)},
	{MT6881_CHK_PD_DIS0, spm, 0x0E70, GENMASK(31, 30)},
	{MT6881_CHK_PD_MM_INFRA, spm, 0x0E78, GENMASK(31, 30)},
	{MT6881_CHK_PD_MM_PROC, spm, 0x0E7C, GENMASK(31, 30)},
	{MT6881_CHK_PD_CSI_RX, spm, 0x0E9C, GENMASK(31, 30)},
	{MT6881_CHK_PD_SSUSB, spm, 0x0EA8, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6881_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

#if BYPASS_SUSPEND_CLK_PWR_CHK
static int off_mtcmos_id[] = {
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6881_CHK_PD_AUDIO,
	MT6881_CHK_PD_ISP_MAIN,
	MT6881_CHK_PD_ISP_DIP1,
	MT6881_CHK_PD_ISP_VCORE,
	MT6881_CHK_PD_VDE0,
	MT6881_CHK_PD_VEN0,
	MT6881_CHK_PD_CAM_MAIN,
	MT6881_CHK_PD_CAM_SUBA,
	MT6881_CHK_PD_CAM_SUBB,
	MT6881_CHK_PD_CAM_VCORE,
	MT6881_CHK_PD_DIS0,
	MT6881_CHK_PD_MM_INFRA,
	MT6881_CHK_PD_MM_PROC,
	MT6881_CHK_PD_CSI_RX,
	MT6881_CHK_PD_SSUSB,
	PD_NULL,
};
#else
static int off_mtcmos_id[] = {
	MT6881_CHK_PD_ISP_MAIN,
	MT6881_CHK_PD_ISP_DIP1,
	MT6881_CHK_PD_ISP_VCORE,
	MT6881_CHK_PD_VDE0,
	MT6881_CHK_PD_VEN0,
	MT6881_CHK_PD_CAM_MAIN,
	MT6881_CHK_PD_CAM_SUBA,
	MT6881_CHK_PD_CAM_SUBB,
	MT6881_CHK_PD_CAM_VCORE,
	MT6881_CHK_PD_DIS0,
	MT6881_CHK_PD_MM_INFRA,
	MT6881_CHK_PD_MM_PROC,
	MT6881_CHK_PD_CSI_RX,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6881_CHK_PD_AUDIO,
	MT6881_CHK_PD_SSUSB,
	PD_NULL,
};
#endif

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) && (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static int suspend_allow_id[] = {

	PD_NULL,
};

static int *get_suspend_allow_id(void)
{
	return suspend_allow_id;
}

static bool pdchk_is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static void check_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6881_reg_value(hwv, HWV_IRQ_STATUS); // FIXME

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6881_CHK_PD_NUM, 0);
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6881_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.external_dump = external_dump,
	.get_pd_pwr_status = get_pd_pwr_status,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.get_suspend_allow_id = get_suspend_allow_id,
	.trace_power_event = trace_power_event,
	.dump_power_event = dump_power_event,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.is_suspend_retry_stop = pdchk_is_suspend_retry_stop,
};

static int pd_chk_mt6881_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6881_ops);
	pdchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6881[] = {
	{
		.compatible = "mediatek,mt6881-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6881_drv = {
	.probe = pd_chk_mt6881_probe,
	.driver = {
		.name = "pd-chk-mt6881",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6881,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6881_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6881_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
