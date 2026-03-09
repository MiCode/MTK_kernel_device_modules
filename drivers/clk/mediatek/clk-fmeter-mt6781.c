// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6781-fmeter.h"

#define FM_TIMEOUT		30
//static DEFINE_SPINLOCK(meter_lock);
//#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
//#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

/*
 * clk fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "hd_faxi_ck", 1),
	FMCLK(CKGEN,  2, "hf_fscp_ck", 1),
	FMCLK(CKGEN,  3, "hf_fmfg_ck", 1),
	FMCLK(CKGEN,  4, "f_fcamtg_ck", 1),
	FMCLK(CKGEN,  5, "f_fcamtg1_ck", 1),
	FMCLK(CKGEN,  6, "f_fcamtg2_ck", 1),
	FMCLK(CKGEN,  7, "f_fcamtg3_ck", 1),
	FMCLK(CKGEN,  8, "f_fcamtg4_ck", 1),
	FMCLK(CKGEN,  9, "f_fcamtg5_ck", 1),
	FMCLK(CKGEN,  10, "f_fcamtg6_ck", 1),
	FMCLK(CKGEN,  11, "f_fuart_ck", 1),
	FMCLK(CKGEN,  12, "hf_fspi_ck", 1),
	FMCLK(CKGEN,  13, "hf_fmsdc50_0_hclk_ck", 1),
	FMCLK(CKGEN,  14, "hf_fmsdc50_0_ck", 1),
	FMCLK(CKGEN,  15, "hf_fmsdc30_1_ck", 1),
	FMCLK(CKGEN,  16, "hf_faudio_ck", 1),
	FMCLK(CKGEN,  17, "hf_faud_intbus_ck", 1),
	FMCLK(CKGEN,  18, "hf_faud_1_ck", 1),
	FMCLK(CKGEN,  19, "hf_faud_2_ck", 1),
	FMCLK(CKGEN,  20, "hf_faud_engen1_ck", 1),
	FMCLK(CKGEN,  21, "hf_faud_engen2_ck", 1),
	FMCLK(CKGEN,  22, "f_fdisp_pwm_ck", 1),
	FMCLK(CKGEN,  23, "hf_sspm_ck", 1),
	FMCLK(CKGEN,  24, "hf_fdxcc_ck", 1),
	FMCLK(CKGEN,  25, "hf_fusb_top_ck", 1),
	FMCLK(CKGEN,  26, "hf_fsrck_ck", 1),
	FMCLK(CKGEN,  27, "hf_fspm_ck", 1),
	FMCLK(CKGEN,  28, "hf_fi2c_ck", 1),
	FMCLK(CKGEN,  29, "f_fpwm_ck", 1),
	FMCLK(CKGEN,  34, "hdf_faes_msdcfde_ck", 1),
	FMCLK(CKGEN,  30, "f_fseninf_ck", 1),
	FMCLK(CKGEN,  31, "f_fseninf1_ck", 1),
	FMCLK(CKGEN,  32, "f_fseninf2_ck", 1),
	FMCLK(CKGEN,  33, "f_fseninf3_ck", 1),
	FMCLK(CKGEN,  35, "f_fpwrap_ulposc_ck", 1),
	FMCLK(CKGEN,  36, "f_fcamtm_ck", 1),
	FMCLK(CKGEN,  37, "hf_fvenc_ck", 1),
	FMCLK(CKGEN,  38, "hf_fcam_ck", 1),
	FMCLK(CKGEN,  39, "hf_fimg1_ck", 1),
	FMCLK(CKGEN,  40, "hf_fipe_ck", 1),
	FMCLK(CKGEN,  41, "hf_dpmaif_ck", 1),
	FMCLK(CKGEN,  42, "hf_fvdec_ck", 1),
	FMCLK(CKGEN,  43, "hf_fdisp_ck", 1),
	FMCLK(CKGEN,  44, "hf_fmdp_ck", 1),
	FMCLK(CKGEN,  45, "hf_faudio_h_ck", 1),
	FMCLK(CKGEN,  46, "hf_fufs_ck", 1),
	FMCLK(CKGEN,  47, "hf_faes_fde_ck", 1),
	FMCLK(CKGEN,  48, "hf_audiodsp_ck", 1),
	FMCLK(CKGEN,  49, "hg_fdvfsrc_ck", 1),
	FMCLK(CKGEN,  50, "hg_fdvfsrc_ck", 1),
	FMCLK(CKGEN,  51, "hf_dsi_occ_ck", 1),
	FMCLK(CKGEN,  52, "hf_fspmi_mst_ck", 1),
	FMCLK(ABIST,  7, "AD_CCIPLL_CK", 1),
	FMCLK(ABIST,  8, "AD_ARMPLL_L_CK", 1),
	FMCLK(ABIST,  9, "AD_ARMPLL_CK", 1),
	FMCLK(ABIST,  10, "AD_PLLGP1_TST_CK", 1),
	FMCLK(ABIST,  11, "AD_MDBPIPLL_CK", 1),
	FMCLK(ABIST,  12, "AD_MDBRPPLL_CK", 1),
	FMCLK(ABIST,  13, "AD_MDVDSPPLL_CK", 1),
	FMCLK(ABIST,  14, "AD_MDMCUPLL_CK", 1),
	FMCLK(ABIST,  15, "AD_APLL2_CK", 1),
	FMCLK(ABIST,  16, "AD_APLL1_CK", 1),
	FMCLK(ABIST,  17, "AD_USB20_192M_CK", 1),
	FMCLK(ABIST,  19, "AD_CCIPLL_CK", 1),
	FMCLK(ABIST,  20, "AD_DSI0_LNTC_DSICLK", 1),
	FMCLK(ABIST,  21, "AD_DSI0_MPPLL_TST_CK", 1),
	FMCLK(ABIST,  24, "AD_MAINPLL_CK", 1),
	FMCLK(ABIST,  25, "AD_MDPLL1_FS26M_CK_guide", 1),
	FMCLK(ABIST,  26, "AD_MFGPLL_CK", 1),
	FMCLK(ABIST,  27, "AD_MMPLL_CK", 1),
	FMCLK(ABIST,  28, "AD_ADSPPLL_CK", 1),
	FMCLK(ABIST,  29, "AD_MPLL_208M_CK", 1),
	FMCLK(ABIST,  30, "AD_MSDCPLL_CK", 1),
	FMCLK(ABIST,  34, "AD_ULPOSC2_CK", 1),
	FMCLK(ABIST,  35, "AD_ULPOSC_CK", 1),
	FMCLK(ABIST,  36, "AD_UNIVPLL_CK", 1),
	FMCLK(ABIST,  40, "ad_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST,  41, "UFS_MP_CLK2FREQ", 1),
	FMCLK(ABIST,  42, "AD_RCLRPLL_DIV4_CK", 1),
	FMCLK(ABIST,  43, "AD_RPHYPLL_DIV4_CK", 1),
	FMCLK(ABIST,  44, "fmem_ck_aft_dsm_ch0", 1),
	FMCLK(ABIST,  45, "fmem_ck_aft_dsm_ch1", 1),
	FMCLK(ABIST,  46, "fmem_ck_bfe_dcm_ch0", 1),
	FMCLK(ABIST,  47, "fmem_ck_bfe_dcm_ch1", 1),
	FMCLK(ABIST,  48, "mcusys_arm_clk_out_all", 1),
	{},
};



#define _CKGEN(x)		(topck_base + (x))
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

#define _SCPSYS(x)		(spm_base + (x))
#define SPM_PWR_STATUS		_SCPSYS(0x16C)
#define SPM_PWR_STATUS_2ND	_SCPSYS(0x170)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;

const struct fmeter_clk *mt6781_get_fmeter_clks(void)
{
	return fclks;
}


/* need implement ckgen&abist api here */

/* implement done */
static unsigned int mt6781_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0000);

	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}
	return (output * 4);

}

static unsigned int mt6781_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));


	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0000);

	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}
	return (output * 4);
}


static unsigned int mt6781_get_abist2_freq(unsigned int ID)
{
	pr_notice("mt6765 not support abist2 func\n");
	return 0;
}

static unsigned int mt6781_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6781_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6781_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6781_get_abist2_freq(id);

	return FT_NULL;
}

static int mt6781_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return f_fdisp_pwm_ck;
	else if (fid == FID_ULPOSC1)
		return f_fpwrap_ulposc_ck;

	return FID_NULL;
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
	.get_fmeter_clks = mt6781_get_fmeter_clks,
	.get_ckgen_freq = mt6781_get_ckgen_freq,
	.get_abist_freq = mt6781_get_abist_freq,
	.get_abist2_freq = mt6781_get_abist2_freq,
	.get_fmeter_freq = mt6781_get_fmeter_freq,
	.get_fmeter_id = mt6781_get_fmeter_id,
};


static int clk_fmeter_mt6781_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,mt6781-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6781-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6781-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6781_drv = {
	.probe = clk_fmeter_mt6781_probe,
	.driver = {
		.name = "clk-fmeter-mt6781",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6781", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6781_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6781_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
