// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kenny Liu <kenny.liu@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "clk-mtk.h"
#if IS_ENABLED(CONFIG_MTK_HWCCF)
#include "hwccf_provider.h"
#include "hwccf_provider_data.h"
#endif
#include "mtk-mminfra-util.h"

struct mminfra_mtcmos {
	u32 voter;
	u32 vote_bit;
	atomic_t ref_cnt;
};

struct mtk_mminfra_pd {
	u32 mminfra_mtcmos_num;
	u32 mminfra_type_num;
	u32 mminfra_type0_pwr;
	struct mminfra_mtcmos mm_mtcmos[MM_PWR_NUM_NR];
};

static struct device *g_dev;
static struct mtk_mminfra_pd *g_mminfra_pd;
spinlock_t mminfra_pd_lock;

#if IS_ENABLED(CONFIG_MTK_HWCCF)
int mminfra0_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_0].voter,
			HWCCF_VOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_0].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_0].voter,
			HWCCF_UNVOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_0].vote_bit);
	}

	return ret;
}

int mminfra1_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_1].voter,
			HWCCF_VOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_1].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_1].voter,
			HWCCF_UNVOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_1].vote_bit);
	}

	return ret;
}

int mminfra_ao_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_AO].voter,
			HWCCF_VOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_AO].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_AO].voter,
			HWCCF_UNVOTE, g_mminfra_pd->mm_mtcmos[MM_PWR_MM_AO].vote_bit);
	}

	return ret;
}

int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type)
{
	int ret = 0, ref_cnt;
	unsigned long flags;

	spin_lock_irqsave(&mminfra_pd_lock, flags);

	// check mminfra_pd valid
	if (!g_mminfra_pd) {
		pr_notice("%s: invalid mminfra_pd\n", __func__);
		spin_unlock_irqrestore(&mminfra_pd_lock, flags);
		return -EINVAL;
	}

	// check mm_pwr valid
	if (mm_pwr >= g_mminfra_pd->mminfra_mtcmos_num) {
		pr_notice("%s: invalid mm_pwr(%d)\n", __func__, mm_pwr);
		spin_unlock_irqrestore(&mminfra_pd_lock, flags);
		return -EINVAL;
	}

	// check mm_type valid
	if (mm_type >= g_mminfra_pd->mminfra_type_num) {
		pr_notice("%s: invalid mm_type(%d)\n", __func__, mm_type);
		spin_unlock_irqrestore(&mminfra_pd_lock, flags);
		return -EINVAL;
	}

	if (on_off) {
		// add ref_cnt
		ref_cnt = atomic_inc_return(&g_mminfra_pd->mm_mtcmos[mm_pwr].ref_cnt);
		if (ref_cnt != 1) {
			pr_notice("%s: already enabled, mm_pwr(%d) ref_cnt(%d)\n",
				__func__, mm_pwr, ref_cnt);
			spin_unlock_irqrestore(&mminfra_pd_lock, flags);
			return 0;
		}
		// power on mminfra
		if (mm_pwr == MM_PWR_MM_0)
			ret = mminfra0_onoff(true);
		else if (mm_pwr == MM_PWR_MM_1)
			ret = mminfra1_onoff(true);
		else if (mm_pwr == MM_PWR_MM_AO)
			ret = mminfra_ao_onoff(true);
	} else {
		// minus ref_cnt
		ref_cnt = atomic_dec_return(&g_mminfra_pd->mm_mtcmos[mm_pwr].ref_cnt);
		if (ref_cnt != 0) {
			pr_notice("%s: ref_cnt>1, mm_pwr(%d) ref_cnt(%d)\n",
				__func__, mm_pwr, ref_cnt);
			spin_unlock_irqrestore(&mminfra_pd_lock, flags);
			return 0;
		}
		// power off mminfra
		if (mm_pwr == MM_PWR_MM_0)
			ret = mminfra0_onoff(false);
		else if (mm_pwr == MM_PWR_MM_1)
			ret = mminfra1_onoff(false);
		else if (mm_pwr == MM_PWR_MM_AO)
			ret = mminfra_ao_onoff(false);
	}

	if (ret) {
		pr_notice("%s: power(%d) on_off(%d) fail, type(%d)\n",
			__func__, mm_pwr, on_off, mm_type);
	}

	spin_unlock_irqrestore(&mminfra_pd_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mminfra_on_off);

int mminfra_ctrl(struct cb_params *cb_para)
{
	pr_notice("%s: name(%s) on_off(%d) vote_bit(%d)\n", __func__,
		cb_para->name, cb_para->onoff, cb_para->vote_bit);

	mtk_mminfra_on_off((cb_para->onoff ? true : false),
		g_mminfra_pd->mminfra_type0_pwr,
		MM_TYPE_CG_LINK);

	return 0;
}
#endif

static int mminfra_util_probe(struct platform_device *pdev)
{
	u32 i, tmp;
	int ret = 0;

	g_mminfra_pd = devm_kzalloc(&pdev->dev, sizeof(*g_mminfra_pd), GFP_KERNEL);
	if (!g_mminfra_pd)
		return -ENOMEM;

	g_dev = &pdev->dev;

	of_property_read_u32(g_dev->of_node, "mminfra-mtcmos-num",
		&g_mminfra_pd->mminfra_mtcmos_num);
	of_property_read_u32(g_dev->of_node, "mminfra-type-num",
		&g_mminfra_pd->mminfra_type_num);
	of_property_read_u32(g_dev->of_node, "mminfra-type0-pwr",
		&g_mminfra_pd->mminfra_type0_pwr);
	for (i = 0; i < MM_PWR_NUM_NR; i++) {
		if (i >= g_mminfra_pd->mminfra_mtcmos_num)
			break;
		if (!of_property_read_u32_index(g_dev->of_node, "mminfra-mtcmos-voter", i, &tmp)) {
			g_mminfra_pd->mm_mtcmos[i].voter = tmp;
			pr_notice("[mminfra] mm[%d] voter(%d)\n",
				i, g_mminfra_pd->mm_mtcmos[i].voter);
		}
		if (!of_property_read_u32_index(g_dev->of_node, "mminfra-mtcmos-data", i, &tmp)) {
			g_mminfra_pd->mm_mtcmos[i].vote_bit = tmp;
			atomic_set(&g_mminfra_pd->mm_mtcmos[i].ref_cnt, 0);
			pr_notice("[mminfra] mm[%d] vote_bit(%d)\n",
				i, g_mminfra_pd->mm_mtcmos[i].vote_bit);
		}
	}

	spin_lock_init(&mminfra_pd_lock);
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	ret = register_mtk_clk_external_api_cb(CLK_REQUEST_MMINFRA_CB, &mminfra_ctrl, NULL);
	if (ret < 0) {
		pr_notice("[mminfra] register mminfra callback fail!\n");
	}
#endif

	return 0;
};

static const struct of_device_id of_mminfra_util_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-util",
	},
	{}
};

static struct platform_driver mminfra_util_drv = {
	.probe = mminfra_util_probe,
	.driver = {
		.name = "mtk-mminfra-util",
		.of_match_table = of_mminfra_util_match_tbl,
	},
};

static int __init mtk_mminfra_util_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_util_drv);
	if (status) {
		pr_notice("Failed to register MMInfra util driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_util_exit(void)
{
	platform_driver_unregister(&mminfra_util_drv);
}

module_init(mtk_mminfra_util_init);
module_exit(mtk_mminfra_util_exit);
MODULE_DESCRIPTION("MTK MMInfra util driver");
MODULE_AUTHOR("Kenny Liu<kenny.liu@mediatek.com>");
MODULE_LICENSE("GPL v2");