// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/kthread.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/mfd/mt6377/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/sort.h>
#include <linux/suspend.h>
#include "mtk_low_battery_throttling.h"
#include "mtk_dynamic_loading_throttling.h"

#define POWER_UVLO_VOLT_LEVEL		2600
#define IMAX_MAX_VALUE			5500
#define DLPT_NOTIFY_FAST_UISOC		30
#define	DLPT_VOLT_MIN			3100

struct reg_t {
	unsigned int addr;
	unsigned int mask;
	unsigned int shift;
};

struct dlpt_regs_t {
	struct reg_t rgs_chrdet;
	struct reg_t uvlo_reg;
	struct reg_t vbb_uvlo_reg;
	const struct linear_range uvlo_range;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct dlpt_regs_t mt6357_dlpt_regs = {
	.rgs_chrdet = {
		MT6357_RGS_CHRDET_ADDR,
		MT6357_RGS_CHRDET_MASK << MT6357_RGS_CHRDET_SHIFT,
		MT6357_RGS_CHRDET_SHIFT
	},
	.uvlo_reg = {
		MT6357_RG_UVLO_VTHL_ADDR,
		MT6357_RG_UVLO_VTHL_MASK << MT6357_RG_UVLO_VTHL_SHIFT,
		MT6357_RG_UVLO_VTHL_SHIFT
	},
	.uvlo_range = {
		.min = 2500,
		.min_sel = 0,
		.max_sel = 8,
		.step = 50,
	},
};

struct dlpt_regs_t mt6358_dlpt_regs = {
	.rgs_chrdet = {
		MT6358_RGS_CHRDET_ADDR,
		MT6358_RGS_CHRDET_MASK << MT6358_RGS_CHRDET_SHIFT,
		MT6358_RGS_CHRDET_SHIFT
	},
	.uvlo_reg = {
		MT6358_RG_UVLO_VTHL_ADDR,
		MT6358_RG_UVLO_VTHL_MASK << MT6358_RG_UVLO_VTHL_SHIFT,
		MT6358_RG_UVLO_VTHL_SHIFT
	},
	.uvlo_range = {
		.min = 2500,
		.min_sel = 0,
		.max_sel = 8,
		.step = 50,
	},
};

struct dlpt_regs_t mt6359p_dlpt_regs = {
	.rgs_chrdet = {
		MT6359P_RGS_CHRDET_ADDR,
		MT6359P_RGS_CHRDET_MASK << MT6359P_RGS_CHRDET_SHIFT,
		MT6359P_RGS_CHRDET_SHIFT
	},
	.uvlo_reg = {
		MT6359P_RG_UVLO_VTHL_ADDR,
		MT6359P_RG_UVLO_VTHL_MASK << MT6359P_RG_UVLO_VTHL_SHIFT,
		MT6359P_RG_UVLO_VTHL_SHIFT
	},
	.uvlo_range = {
		.min = 2500,
		.min_sel = 0,
		.max_sel = 8,
		.step = 50,
	},
};

struct dlpt_regs_t mt6363_dlpt_regs = {
	.rgs_chrdet = {
		MT6363_CHRDET_DEB_ADDR,
		MT6363_CHRDET_DEB_MASK << MT6363_CHRDET_DEB_SHIFT,
		MT6363_CHRDET_DEB_SHIFT
	},
	.uvlo_reg = {
		MT6363_RG_VSYS_UVLO_VTHL_ADDR,
		MT6363_RG_VSYS_UVLO_VTHL_MASK << MT6363_RG_VSYS_UVLO_VTHL_SHIFT,
		MT6363_RG_VSYS_UVLO_VTHL_SHIFT
	},
	.vbb_uvlo_reg = {
		MT6363_RG_VBB_UVLO_VTHL_ADDR,
		MT6363_RG_VBB_UVLO_VTHL_MASK << MT6363_RG_VBB_UVLO_VTHL_SHIFT,
		MT6363_RG_VBB_UVLO_VTHL_SHIFT
	},
	.uvlo_range = {
		.min = 2000,
		.min_sel = 0,
		.max_sel = 9,
		.step = 100,
	},
};

struct dlpt_regs_t mt6377_dlpt_regs = {
	.rgs_chrdet = {
		MT6377_CHRDET_DEB_ADDR,
		MT6377_CHRDET_DEB_MASK << MT6377_CHRDET_DEB_SHIFT,
		MT6377_CHRDET_DEB_SHIFT
	},
	.uvlo_reg = {
		MT6377_RG_VSYS_UVLO_VTHL_ADDR,
		MT6377_RG_VSYS_UVLO_VTHL_MASK << MT6377_RG_VSYS_UVLO_VTHL_SHIFT,
		MT6377_RG_VSYS_UVLO_VTHL_SHIFT
	},
	.uvlo_range = {
		.min = 2500,
		.min_sel = 0,
		.max_sel = 8,
		.step = 50,
	},
};

struct dlpt_priv {
	struct regmap *regmap;
	enum LOW_BATTERY_LEVEL_TAG lbat_level;
	const struct dlpt_regs_t *regs;
	/* dlpt notify */
	struct mutex notify_lock;
	struct wakeup_source *notify_ws;
	struct timer_list notify_timer;
	struct wait_queue_head notify_waiter;
	struct task_struct *notify_thread;
	bool notify_flag;
	/* Imix */
	int imix;
	int imix_r;
	/* others */
	int is_power_path_supported;
	int is_isense_supported;
	struct iio_channel *chan_ptim;
	struct iio_channel *chan_imix_r;
	struct iio_channel *chan_zcv;
	bool suspend_flag;
	struct tag_bootmode *tag;
};

struct dlpt_callback_table {
	void (*dlptcb)(int value);
};

static struct dlpt_priv dlpt = {
	.notify_lock	=  __MUTEX_INITIALIZER(dlpt.notify_lock),
	.notify_waiter	= __WAIT_QUEUE_HEAD_INITIALIZER(dlpt.notify_waiter),
	.suspend_flag = false,
};
#define DLPTCB_MAX_NUM 16
static struct dlpt_callback_table dlptcb_tb[DLPTCB_MAX_NUM] = { {0} };

/*
 * Get ZCV/imix_r Auxadc function
 */
static void update_dlpt_imix_r(void)
{
	int ret = 0;

	if (!PTR_ERR_OR_ZERO(dlpt.chan_imix_r)) {
		ret = iio_read_channel_raw(dlpt.chan_imix_r, &dlpt.imix_r);
		if (ret < 0) {
			pr_notice("[%s] iio_read_channel_raw error\n", __func__);
			return;
		}
	}
	pr_info("[dlpt] imix_r=%d\n", dlpt.imix_r);
}

static int dlpt_adc_chan_init(struct platform_device *pdev)
{
	int ret = 0;

	dlpt.chan_ptim = devm_iio_channel_get(&pdev->dev, "pmic_ptim");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_ptim);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s ptim fail, ret=%d\n", __func__, ret);
		return ret;
	}

	dlpt.chan_imix_r = devm_iio_channel_get(&pdev->dev, "pmic_imix_r");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_imix_r);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s imix_r fail, ret=%d\n", __func__, ret);
		return ret;
	}
	update_dlpt_imix_r();

	/* direct point to BATADC or ISENSE phandle in DTS */
	if (dlpt.is_isense_supported && dlpt.is_power_path_supported)
		dlpt.chan_zcv = devm_iio_channel_get(&pdev->dev, "pmic_isense");
	else
		dlpt.chan_zcv = devm_iio_channel_get(&pdev->dev, "pmic_batadc");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_zcv);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s pmic_zcv fail, ret=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

/*
 * DLPT notify function
 */
void register_dlpt_notify(dlpt_callback dlpt_cb,
			  enum DLPT_PRIO_TAG prio_val)
{
	if (prio_val >= DLPTCB_MAX_NUM) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return;
	}
	dlptcb_tb[prio_val].dlptcb = dlpt_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (dlpt.imix != 0) {
		pr_notice("[%s] happen\n", __func__);
		if (dlpt_cb != NULL)
			dlpt_cb(dlpt.imix);
	}
}
EXPORT_SYMBOL(register_dlpt_notify);

#define	IMP_VOLT_TENFOLD	(10)
static int linear_range_get_selector(const struct linear_range *r,
				     unsigned int val, unsigned int *selector)
{
	if ((r->min + (r->max_sel - r->min_sel) * r->step) < val)
		return -EINVAL;

	if (r->min > val) {
		*selector = r->min_sel;
		return 0;
	}
	if (r->step == 0)
		*selector = r->max_sel;
	else
		*selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

	return 0;
}

static void pmic_uvlo_init(int uvlo_level, int vbb_uvlo_level)
{
	int ret, val = 0;

	ret = linear_range_get_selector(&dlpt.regs->uvlo_range, uvlo_level, &val);
	if (!ret) {
		regmap_update_bits(dlpt.regmap, dlpt.regs->uvlo_reg.addr,
				   dlpt.regs->uvlo_reg.mask,
				   val << dlpt.regs->uvlo_reg.shift);
		pr_info("[dlpt] UVLO_VOLT_LEVEL = %d, RG_UVLO_VTHL = 0x%x\n",
			uvlo_level, val);
	} else
		pr_notice("[dlpt] Invalid uvlo_level (%d)\n", uvlo_level);

	if (vbb_uvlo_level) {
		ret = linear_range_get_selector(&dlpt.regs->uvlo_range, vbb_uvlo_level, &val);
		if (!ret) {
			regmap_update_bits(dlpt.regmap, dlpt.regs->vbb_uvlo_reg.addr,
					   dlpt.regs->vbb_uvlo_reg.mask,
					   val << dlpt.regs->vbb_uvlo_reg.shift);
			pr_info("[dlpt] VBB_UVLO_VOLT_LEVEL = %d, RG_VBB_UVLO_VTHL = 0x%x\n",
				vbb_uvlo_level, val);
		} else
			pr_notice("[dlpt] Invalid vbb_uvlo_level (%d)\n", vbb_uvlo_level);
	}
}

static void dlpt_parse_dt(struct platform_device *pdev)
{
	struct device_node *np, *rt6160_np;
	int uvlo_level = 0, vbb_uvlo_level;
	int bob_check_flag = 0, bob_exist = 0;
	int ret;

	/* get dlpt device node */
	np = pdev->dev.of_node;
	if (!np)
		dev_notice(&pdev->dev, "get dlpt node fail\n");
	else {
		/* get isense support */
		dlpt.is_isense_supported =
			of_property_read_bool(np, "isense_support");
		/* get uvlo-level */
		ret = of_property_read_u32(np, "uvlo-level", &uvlo_level);
		if (ret)
			uvlo_level = POWER_UVLO_VOLT_LEVEL;
		/* get vbb-uvlo-level */
		ret = of_property_read_u32(np, "vbb-uvlo-level", &vbb_uvlo_level);
		if (ret)
			vbb_uvlo_level = 0;

		ret = of_property_read_u32(np, "bob-check-flag", &bob_check_flag);
		if (ret)
			bob_check_flag = 0;
		if (bob_check_flag) {
			rt6160_np = of_find_node_by_name(NULL, "rt6160");
			if (rt6160_np) {
				ret = of_property_read_u32(rt6160_np, "is-existed", &bob_exist);
				if (ret)
					bob_exist = 0;
				dev_notice(&pdev->dev, "bob_exist:%d\n", bob_exist);
				if (!bob_exist) {
					uvlo_level = 2600;
					vbb_uvlo_level = 2500;
				}
			} else {
				dev_notice(&pdev->dev, "get rt6160 node fail\n");
			}
		}
		pmic_uvlo_init(uvlo_level, vbb_uvlo_level);

		/* get power_path_support */
		np = of_parse_phandle(pdev->dev.of_node, "mediatek,charger", 0);
		if (!np)
			dev_notice(&pdev->dev, "get charger node fail\n");
		else
			dlpt.is_power_path_supported =
				of_property_read_bool(np, "power_path_support");

		np = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
		if (!np)
			dev_notice(&pdev->dev, "get bootmode fail\n");
		else {
			dlpt.tag = (struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
			if (!dlpt.tag)
				dev_notice(&pdev->dev, "failed to get atag,boot\n");
			else
				dev_notice(&pdev->dev, "bootmode:0x%x\n", dlpt.tag->bootmode);
		}
	}
	dev_notice(&pdev->dev, "power_path_support:%d isense_support:%d\n"
		   , dlpt.is_power_path_supported, dlpt.is_isense_supported);
}

static int dlpt_probe(struct platform_device *pdev)
{
	struct mt6397_chip *chip;
	int ret;

	dlpt.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!dlpt.regmap) {
		chip = dev_get_drvdata(pdev->dev.parent);
		if (!chip || !chip->regmap) {
			dev_notice(&pdev->dev, "%s: invalid regmap.\n", __func__);
			return -ENODEV;
		}
		dlpt.regmap = chip->regmap;
	}
	dlpt.regs = of_device_get_match_data(&pdev->dev);
	dlpt_parse_dt(pdev);

	ret = dlpt_adc_chan_init(pdev);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused dlpt_suspend(struct device *d)
{
	if (!mutex_trylock(&dlpt.notify_lock))
		return -EAGAIN;
	dlpt.suspend_flag = true;
	mutex_unlock(&dlpt.notify_lock);
	return 0;
}

static int __maybe_unused dlpt_resume(struct device *d)
{
	mutex_lock(&dlpt.notify_lock);
	dlpt.suspend_flag = false;
	mutex_unlock(&dlpt.notify_lock);
	update_dlpt_imix_r();
	wake_up_interruptible(&dlpt.notify_waiter);
	return 0;
}

static SIMPLE_DEV_PM_OPS(dlpt_pm_ops,
			 dlpt_suspend,
			 dlpt_resume);

static const struct of_device_id dynamic_loading_throttling_of_match[] = {
	{
		.compatible = "mediatek,mt6357-dynamic_loading_throttling",
		.data = &mt6357_dlpt_regs,
	}, {
		.compatible = "mediatek,mt6358-dynamic_loading_throttling",
		.data = &mt6358_dlpt_regs,
	}, {
		.compatible = "mediatek,mt6359p-dynamic_loading_throttling",
		.data = &mt6359p_dlpt_regs,
	}, {
		.compatible = "mediatek,mt6363-dynamic_loading_throttling",
		.data = &mt6363_dlpt_regs,
	}, {
		.compatible = "mediatek,mt6377-dynamic_loading_throttling",
		.data = &mt6377_dlpt_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, dynamic_loading_throttling_of_match);

static struct platform_driver dynamic_loading_throttling_driver = {
	.driver = {
		.name = "mtk_dynamic_loading_throttling",
		.of_match_table = dynamic_loading_throttling_of_match,
		.pm = &dlpt_pm_ops,
	},
	.probe = dlpt_probe,
};
module_platform_driver(dynamic_loading_throttling_driver);

MODULE_AUTHOR("Wen Su <Wen.Su@mediatek.com>");
MODULE_DESCRIPTION("MTK dynamic loading throttling driver");
MODULE_LICENSE("GPL");
