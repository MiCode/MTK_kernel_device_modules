// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6858-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6858.h"
#include "clk-fmeter.h"
#include "clk-mt6858-fmeter.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0
#define EVT_LEN				40
#define PWR_ID_SHIFT			0
#define PWR_STA_SHIFT			8
#define HWV_INT_MTCMOS_TRIGGER		0x0008
#define HWV_IRQ_STATUS			0x1500

static DEFINE_SPINLOCK(pwr_trace_lock);
static unsigned int pwr_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

static void trace_power_event(unsigned int id, unsigned int pwr_sta)
{
	unsigned long flags = 0;

	if (id >= MT6858_CHK_PD_NUM)
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
	SWCG("afe_dl0_dac_tml"),
	SWCG("afe_dl0_dac_hires"),
	SWCG("afe_dl0_dac"),
	SWCG("afe_dl0_predis"),
	SWCG("afe_dl0_nle"),
	SWCG("afe_pcm1"),
	SWCG("afe_pcm0"),
	SWCG("afe_cm1"),
	SWCG("afe_cm0"),
	SWCG("afe_stf"),
	SWCG("afe_hw_gain23"),
	SWCG("afe_hw_gain01"),
	SWCG("afe_ul1_aht"),
	SWCG("afe_ul1_adc_hires"),
	SWCG("afe_ul1_tml"),
	SWCG("afe_ul1_adc"),
	SWCG("afe_ul0_aht"),
	SWCG("afe_ul0_adc_hires"),
	SWCG("afe_ul0_tml"),
	SWCG("afe_ul0_adc"),
	SWCG("afe_etdm_in4"),
	SWCG("afe_etdm_in2"),
	SWCG("afe_etdm_in1"),
	SWCG("afe_etdm_out4"),
	SWCG("afe_etdm_out2"),
	SWCG("afe_etdm_out1"),
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
	SWCG(NULL),
};
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ovl2_2l"),
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
	SWCG("mm_disp_dsc_wrap0"),
	SWCG("mm_CLK0"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_disp_apb_bus"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_mutex0"),
	SWCG("mm_smi_common"),
	SWCG("mm_dsi0_ck"),
	SWCG("mm_26m_ck"),
	SWCG(NULL),
};
/* imgsys1 */
struct pd_check_swcg imgsys1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_larb10"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};
/* imgsys2 */
struct pd_check_swcg imgsys2_swcgs[] = {
	SWCG("imgsys2_larb9"),
	SWCG("imgsys2_mfb"),
	SWCG("imgsys2_wpe"),
	SWCG("imgsys2_mss"),
	SWCG("imgsys2_gals"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_lat_cken"),
	SWCG("vde2_lat_active"),
	SWCG("vde2_lat_cken_eng"),
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
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_cam2mm_gals"),
	SWCG("cam_m_pda"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* ipesys */
struct pd_check_swcg ipesys_swcgs[] = {
	SWCG("ipe_larb20"),
	SWCG("ipe_smi_subcom"),
	SWCG("ipe_fd"),
	SWCG("ipe_rsc"),
	SWCG("ipe_gals"),
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
	SWCG("mdp_rsz2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_dlo_async0"),
	SWCG("mdp_fmm_img_dl_async0"),
	SWCG("mdp_fmm_img_dl_async1"),
	SWCG("mdp_fimg_img_dl_async0"),
	SWCG("mdp_fimg_img_dl_async1"),
	SWCG(NULL),
};
/* mminfra_config */
struct pd_check_swcg mminfra_config_swcgs[] = {
	SWCG("mminfra_gce_d"),
	SWCG("mminfra_gce_m"),
	SWCG("mminfra_gce_26m"),
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
	{MT6858_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6858_CHK_PD_DIS0, MT6858_CHK_PD_MM_INFRA, dispsys_config_swcgs, mm},
	{MT6858_CHK_PD_ISP_IMG1, MT6858_CHK_PD_MM_INFRA, imgsys1_swcgs, imgsys1},
	{MT6858_CHK_PD_ISP_IMG2, MT6858_CHK_PD_ISP_IMG1, imgsys2_swcgs, imgsys2},
	{MT6858_CHK_PD_VDE0, MT6858_CHK_PD_MM_INFRA, vdec_gcon_base_swcgs, vde2},
	{MT6858_CHK_PD_VEN0, MT6858_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6858_CHK_PD_CAM_MAIN, MT6858_CHK_PD_MM_INFRA, camsys_main_swcgs, cam_m},
	{MT6858_CHK_PD_CAM_SUBA, MT6858_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6858_CHK_PD_CAM_SUBB, MT6858_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6858_CHK_PD_ISP_IPE, MT6858_CHK_PD_MM_INFRA, ipesys_swcgs, ipe},
	{MT6858_CHK_PD_DIS0, MT6858_CHK_PD_MM_INFRA, mdpsys_config_swcgs, mdp},
	{MT6858_CHK_PD_MM_INFRA, PD_NULL, mminfra_config_swcgs, mminfra_config},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6858_CHK_PD_NUM)
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

	if (id >= MT6858_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6858(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6858_CHK_PD_CONN,
	MT6858_CHK_PD_AUDIO,
	MT6858_CHK_PD_ISP_IMG1,
	MT6858_CHK_PD_ISP_IMG2,
	MT6858_CHK_PD_ISP_IPE,
	MT6858_CHK_PD_VDE0,
	MT6858_CHK_PD_VEN0,
	MT6858_CHK_PD_CAM_MAIN,
	MT6858_CHK_PD_CAM_SUBA,
	MT6858_CHK_PD_CAM_SUBB,
	MT6858_CHK_PD_DIS0,
	MT6858_CHK_PD_MM_INFRA,
	MT6858_CHK_PD_MM_PROC,
	MT6858_CHK_PD_CSI_RX,
	MT6858_CHK_PD_SSUSB,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6858_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static enum chk_sys_id debug_dump_id[] = {
	spm,
	top,
	apmixed,
	vlpcfg_reg_bus,
	vlp_top,
	hwv,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	const struct fmeter_clk *fclks;
	int i, parent_id = PD_NULL;

	if (id >= MT6858_CHK_PD_NUM)
		return;

	fclks = mt_get_fmeter_clks();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	dump_power_event();

	set_subsys_reg_dump_mt6858(debug_dump_id);

	get_subsys_reg_dump_mt6858();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id) {
			print_subsys_reg_mt6858(mtk_subsys_check[i].chk_id);
			parent_id = mtk_subsys_check[i].pd_parent;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (parent_id == PD_NULL)
			break;

		if (mtk_subsys_check[i].pd_id == parent_id)
			print_subsys_reg_mt6858(mtk_subsys_check[i].chk_id);
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

	set_subsys_reg_dump_mt6858(debug_dump_id);
	get_subsys_reg_dump_mt6858();
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6858_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6858_CHK_PD_AUDIO, spm, 0x0E18, GENMASK(31, 30)},
	{MT6858_CHK_PD_ISP_IMG1, spm, 0x0E28, GENMASK(31, 30)},
	{MT6858_CHK_PD_ISP_IMG2, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6858_CHK_PD_ISP_IPE, spm, 0x0E30, GENMASK(31, 30)},
	{MT6858_CHK_PD_VDE0, spm, 0x0E34, GENMASK(31, 30)},
	{MT6858_CHK_PD_VEN0, spm, 0x0E3C, GENMASK(31, 30)},
	{MT6858_CHK_PD_CAM_MAIN, spm, 0x0E44, GENMASK(31, 30)},
	{MT6858_CHK_PD_CAM_SUBA, spm, 0x0E4C, GENMASK(31, 30)},
	{MT6858_CHK_PD_CAM_SUBB, spm, 0x0E50, GENMASK(31, 30)},
	{MT6858_CHK_PD_DIS0, spm, 0x0E6C, GENMASK(31, 30)},
	{MT6858_CHK_PD_MM_INFRA, spm, 0x0E74, GENMASK(31, 30)},
	{MT6858_CHK_PD_MM_PROC, spm, 0x0E78, GENMASK(31, 30)},
	{MT6858_CHK_PD_CSI_RX, spm, 0x0E98, GENMASK(31, 30)},
	{MT6858_CHK_PD_SSUSB, spm, 0x0EA4, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6858_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
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
	MT6858_CHK_PD_ISP_IMG1,
	MT6858_CHK_PD_ISP_IMG2,
	MT6858_CHK_PD_ISP_IPE,
	MT6858_CHK_PD_VDE0,
	MT6858_CHK_PD_VEN0,
	MT6858_CHK_PD_CAM_MAIN,
	MT6858_CHK_PD_CAM_SUBA,
	MT6858_CHK_PD_CAM_SUBB,
	MT6858_CHK_PD_DIS0,
	MT6858_CHK_PD_MM_INFRA,
	MT6858_CHK_PD_MM_PROC,
	MT6858_CHK_PD_CSI_RX,
	MT6858_CHK_PD_CONN,
	MT6858_CHK_PD_AUDIO,
	MT6858_CHK_PD_SSUSB,
	PD_NULL,
};
#else
static int off_mtcmos_id[] = {
	MT6858_CHK_PD_ISP_IMG1,
	MT6858_CHK_PD_ISP_IMG2,
	MT6858_CHK_PD_ISP_IPE,
	MT6858_CHK_PD_VDE0,
	MT6858_CHK_PD_VEN0,
	MT6858_CHK_PD_CAM_MAIN,
	MT6858_CHK_PD_CAM_SUBA,
	MT6858_CHK_PD_CAM_SUBB,
	MT6858_CHK_PD_DIS0,
	MT6858_CHK_PD_MM_INFRA,
	MT6858_CHK_PD_MM_PROC,
	MT6858_CHK_PD_CSI_RX,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6858_CHK_PD_CONN,
	MT6858_CHK_PD_AUDIO,
	MT6858_CHK_PD_SSUSB,
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

	irq_sta = get_mt6858_reg_value(hwv, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6858_CHK_PD_NUM, 0);
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6858_ops = {
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

static int pd_chk_mt6858_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6858_ops);
	pdchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6858[] = {
	{
		.compatible = "mediatek,mt6858-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6858_drv = {
	.probe = pd_chk_mt6858_probe,
	.driver = {
		.name = "pd-chk-mt6858",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6858,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6858_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6858_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
