
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/power/mt6768-power.h>
#include "mtk-pd-chk.h"
#include "clkchk-mt6768.h"
#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0

static unsigned int suspend_cnt;

/*
 * The clk names in Mediatek CCF.
 */
/* mmsys */
struct pd_check_swcg mm_swcgs[] = {
	SWCG("mm_mdp_rdma0"),
	SWCG("mm_mdp_ccorr0"),
	SWCG("mm_mdp_rsz0"),
	SWCG("mm_mdp_rsz1"),
	SWCG("mm_mdp_tdshp0"),
	SWCG("mm_mdp_wrot0"),
	SWCG("mm_mdp_wdma0"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_dsi0"),
	SWCG("mm_fake_eng"),
	SWCG("mm_smi_common"),
	SWCG("mm_smi_larb0"),
	SWCG("mm_smi_comm0"),
	SWCG("mm_smi_comm1"),
	SWCG("mm_cam_mdp_ck"),
	SWCG("mm_smi_img_ck"),
	SWCG("mm_smi_cam_ck"),
	SWCG("mm_smi_venc_ck"),
	SWCG("mm_smi_vdec_ck"),
	SWCG("mm_img_dl_relay"),
	SWCG("mm_imgdl_async"),
	SWCG("mm_dig_dsi_ck"),
	SWCG("mm_hrtwt"),
	SWCG(NULL),
};
/* img */
struct pd_check_swcg img_swcgs[] = {
	SWCG("img_larb2"),
	SWCG("img_dip"),
	SWCG("img_fdvt"),
	SWCG("img_dpe"),
	SWCG("img_rsc"),
	SWCG(NULL),
};
/* cam */
struct pd_check_swcg cam_swcgs[] = {
	SWCG("cam_larb3"),
	SWCG("cam_dfp_vad"),
	SWCG("cam"),
	SWCG("camtg"),
	SWCG("cam_seninf"),
	SWCG("camsv0"),
	SWCG("camsv1"),
	SWCG("camsv2"),
	SWCG("cam_ccu"),
	SWCG(NULL),
};
/* ven */
struct pd_check_swcg venc_swcgs[] = {
	SWCG("venc_set0_larb"),
	SWCG("venc_set1_venc"),
	SWCG("jpgenc"),
	SWCG(NULL),
};
/* vdec */
struct pd_check_swcg vdec_swcgs[] = {
	SWCG("vdec_cken"),
	SWCG("vdec_active"),
	SWCG("vdec_cken_eng"),
	SWCG("vdec_larb1_cken"),
	SWCG(NULL),
};
struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};
struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6768_POWER_DOMAIN_DISP, mm_swcgs, mmsys},
	{MT6768_POWER_DOMAIN_ISP, img_swcgs, imgsys},
	{MT6768_POWER_DOMAIN_CAM, cam_swcgs, camsys},
	{MT6768_POWER_DOMAIN_VENC, venc_swcgs, vencsys},
	{MT6768_POWER_DOMAIN_VDEC, vdec_swcgs, vdecsys},
};
static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;
	if (id >= MT6768_POWER_DOMAIN_NR)
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
	if (id >= MT6768_POWER_DOMAIN_NR)
		return;
	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6768(mtk_subsys_check[i].chk_id);
	}
}
unsigned int pd_list[] = {
	MT6768_POWER_DOMAIN_MD,
	MT6768_POWER_DOMAIN_CONN,
	MT6768_POWER_DOMAIN_DPY,
	MT6768_POWER_DOMAIN_DISP,
	MT6768_POWER_DOMAIN_MFG,
	MT6768_POWER_DOMAIN_ISP,
	MT6768_POWER_DOMAIN_IFR,
	MT6768_POWER_DOMAIN_MFG_CORE0,
	MT6768_POWER_DOMAIN_MFG_CORE1,
	MT6768_POWER_DOMAIN_MFG_ASYNC,
	MT6768_POWER_DOMAIN_CAM,
	MT6768_POWER_DOMAIN_VENC,
	MT6768_POWER_DOMAIN_VDEC,
};
static bool is_in_pd_list(unsigned int id)
{
	int i;
	if (id >= MT6768_POWER_DOMAIN_NR)
		return false;
	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}
	return false;
}
static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i;
	print_subsys_reg_mt6768(scpsys);
	print_subsys_reg_mt6768(topckgen);
	print_subsys_reg_mt6768(infracfg);
	print_subsys_reg_mt6768(apmixedsys);
	if (id >= MT6768_POWER_DOMAIN_NR)
		return;
	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6768(mtk_subsys_check[i].chk_id);
		}
	}
	BUG_ON(1);
}
static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6768_POWER_DOMAIN_NR)
		return;
	if (id == MT6768_POWER_DOMAIN_MD) {
		print_subsys_reg_mt6768(infracfg);
		print_subsys_reg_mt6768(scpsys);
	}
}
static int off_mtcmos_id[] = {
	MT6768_POWER_DOMAIN_DISP,
	MT6768_POWER_DOMAIN_MFG,
	MT6768_POWER_DOMAIN_ISP,
	MT6768_POWER_DOMAIN_MFG_CORE0,
	MT6768_POWER_DOMAIN_MFG_CORE1,
	MT6768_POWER_DOMAIN_MFG_ASYNC,
	MT6768_POWER_DOMAIN_CAM,
	MT6768_POWER_DOMAIN_VENC,
	MT6768_POWER_DOMAIN_VDEC,
	PD_NULL,
};
static int notice_mtcmos_id[] = {
	MT6768_POWER_DOMAIN_MD,
	MT6768_POWER_DOMAIN_CONN,
	PD_NULL,
};
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
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}
static struct pd_sta pd_pwr_msk[] = {
	{MT6768_POWER_DOMAIN_MD, scpsys, 0x180, BIT(0)},
	{MT6768_POWER_DOMAIN_CONN, scpsys, 0x180, BIT(1)},
	{MT6768_POWER_DOMAIN_DPY, scpsys, 0x180, BIT(2)},
	{MT6768_POWER_DOMAIN_DISP, scpsys, 0x180, BIT(5)},
	{MT6768_POWER_DOMAIN_MFG, scpsys, 0x180, BIT(11)},
	{MT6768_POWER_DOMAIN_ISP, scpsys, 0x180, BIT(6)},
	{MT6768_POWER_DOMAIN_IFR, scpsys, 0x180,  BIT(3)},
	{MT6768_POWER_DOMAIN_MFG_CORE0, scpsys, 0x180, BIT(12)},
	{MT6768_POWER_DOMAIN_MFG_CORE1, scpsys, 0x180, BIT(13)},
	{MT6768_POWER_DOMAIN_MFG_ASYNC, scpsys, 0x180, BIT(14)},
	{MT6768_POWER_DOMAIN_CAM, scpsys, 0x180, BIT(7)},
	{MT6768_POWER_DOMAIN_VENC, scpsys, 0x180, BIT(9)},
	{MT6768_POWER_DOMAIN_VDEC, scpsys, 0x180, BIT(8)},
};
static u32 get_pd_pwr_msk(int pd_id)
{
	u32 val;
	int i;
	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_msk))
		return 0;
	for (i = 0; i < ARRAY_SIZE(pd_pwr_msk); i++) {
		if (pd_id == pd_pwr_msk[i].pd_id) {
			val = get_mt6768_reg_value(pd_pwr_msk[i].base, pd_pwr_msk[i].ofs);
			if ((val & pd_pwr_msk[i].msk) == pd_pwr_msk[i].msk)
				return 1;
			else
				return 0;
		}
	}
	return 0;
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

/*
 * init functions
 */
static struct pdchk_ops pdchk_mt6768_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_status = get_pd_pwr_msk,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.is_suspend_retry_stop = pdchk_is_suspend_retry_stop,
};
static int pd_chk_mt6768_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;
	pdchk_common_init(&pdchk_mt6768_ops);
	return 0;
}
static const struct of_device_id of_match_pdchk_mt6768[] = {
	{
		.compatible = "mediatek,mt6768-pdchk",
	}, {
		/* sentinel */
	}
};
static struct platform_driver pd_chk_mt6768_drv = {
	.probe = pd_chk_mt6768_probe,
	.driver = {
		.name = "pd-chk-mt6768",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6768,
	},
};
/*
 * init functions
 */
static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6768_drv);
}
static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6768_drv);
}
late_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
