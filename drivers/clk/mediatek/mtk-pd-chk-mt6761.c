// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6761-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6761.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0

/*
 * The clk names in Mediatek CCF.
 */

/* mm */
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
	SWCG("mm_disp_rsz"),
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
	SWCG("mm_img_dl_relay"),
	SWCG("mm_imgdl_async"),
	SWCG("mm_dig_dsi_ck"),
	SWCG("mm_hrtwt"),
	SWCG(NULL),
};

/* mfg */
struct pd_check_swcg mfg_swcgs[] = {
	SWCG("mfgcfg_baxi"),
	SWCG("mfgcfg_bmem"),
	SWCG("mfgcfg_bg3d"),
	SWCG("mfgcfg_b26m"),
	SWCG(NULL),
};

/* cam */
struct pd_check_swcg cam_swcgs[] = {
	SWCG("cam_larb2"),
	SWCG("cam"),
	SWCG("camtg"),
	SWCG("cam_seninf"),
	SWCG("camsv0"),
	SWCG("camsv1"),
	SWCG("cam_fdvt"),
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
	{MT6761_POWER_DOMAIN_DIS, mm_swcgs, mmsys},
	{MT6761_POWER_DOMAIN_MFG_ASYNC, mfg_swcgs, mfgsys},
	{MT6761_POWER_DOMAIN_CAM, cam_swcgs, camsys},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6761_POWER_DOMAIN_NR)
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

	if (id >= MT6761_POWER_DOMAIN_NR)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6761(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6761_POWER_DOMAIN_MD,
	MT6761_POWER_DOMAIN_CONN,
	MT6761_POWER_DOMAIN_DPY,
	MT6761_POWER_DOMAIN_DIS,
	MT6761_POWER_DOMAIN_MFG,
	MT6761_POWER_DOMAIN_IFR,
	MT6761_POWER_DOMAIN_MFG_CORE0,
	MT6761_POWER_DOMAIN_MFG_ASYNC,
	MT6761_POWER_DOMAIN_CAM,
	MT6761_POWER_DOMAIN_VCODEC,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6761_POWER_DOMAIN_NR)
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

	print_subsys_reg_mt6761(scpsys);
	print_subsys_reg_mt6761(topckgen);
	print_subsys_reg_mt6761(infracfg);
	print_subsys_reg_mt6761(apmixed);

	if (id >= MT6761_POWER_DOMAIN_NR)
		return;

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6761(mtk_subsys_check[i].chk_id);
		}
	}

	BUG_ON(1);
}

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6761_POWER_DOMAIN_NR)
		return;

	if (id == MT6761_POWER_DOMAIN_MD) {
		print_subsys_reg_mt6761(infracfg);
		print_subsys_reg_mt6761(scpsys);
	}
}

static struct pd_sta pd_pwr_msk[] = {
	{MT6761_POWER_DOMAIN_MD, scpsys, 0x180, BIT(0)},
	{MT6761_POWER_DOMAIN_CONN, scpsys, 0x180, BIT(1)},
	{MT6761_POWER_DOMAIN_DPY, scpsys, 0x180, BIT(2)},
	{MT6761_POWER_DOMAIN_DIS, scpsys, 0x180, BIT(3)},
	{MT6761_POWER_DOMAIN_MFG, scpsys, 0x180, BIT(4)},
	{MT6761_POWER_DOMAIN_IFR, scpsys, 0x180, BIT(6)},
	{MT6761_POWER_DOMAIN_MFG_CORE0, scpsys, 0x180, BIT(7)},
	{MT6761_POWER_DOMAIN_MFG_ASYNC, scpsys, 0x180, BIT(23)},
	{MT6761_POWER_DOMAIN_CAM, scpsys, 0x180, BIT(25)},
	{MT6761_POWER_DOMAIN_VCODEC, scpsys, 0x180, BIT(26)},
};

static u32 get_pd_pwr_msk(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_msk))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_msk); i++) {
		if (pd_id == pd_pwr_msk[i].pd_id) {
			val = get_mt6761_reg_value(pd_pwr_msk[i].base, pd_pwr_msk[i].ofs);
			if ((val & pd_pwr_msk[i].msk) == pd_pwr_msk[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6761_POWER_DOMAIN_DIS,
	MT6761_POWER_DOMAIN_MFG,
	MT6761_POWER_DOMAIN_MFG_CORE0,
	MT6761_POWER_DOMAIN_MFG_ASYNC,
	MT6761_POWER_DOMAIN_CAM,
	MT6761_POWER_DOMAIN_VCODEC,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6761_POWER_DOMAIN_MD,
	MT6761_POWER_DOMAIN_CONN,
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

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6761_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_status = get_pd_pwr_msk,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
};

static int pd_chk_mt6761_probe(struct platform_device *pdev)
{
	pdchk_common_init(&pdchk_mt6761_ops);
	return 0;
}

static const struct of_device_id of_match_pdchk_mt6761[] = {
	{
		.compatible = "mediatek,mt6761-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6761_drv = {
	.probe = pd_chk_mt6761_probe,
	.driver = {
		.name = "pd-chk-mt6761",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6761,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6761_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6761_drv);
}

late_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
