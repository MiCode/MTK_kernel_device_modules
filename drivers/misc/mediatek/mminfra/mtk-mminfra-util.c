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
#include <linux/sched/clock.h>
#include "clk-mtk.h"
#include "clkchk.h"
#if IS_ENABLED(CONFIG_MTK_HWCCF)
#include "hwccf_provider.h"
#include "hwccf_provider_data.h"
#endif
#include "mtk-mminfra-util.h"

#define MM_TIMESTAMP_NUM	(5)

struct mminfra_mtcmos {
	u32 voter;
	u32 vote_bit;
	atomic_t ref_cnt;
	void __iomem *base;
	u32 mask;
	u64 on_t[MM_TIMESTAMP_NUM]; /* ms */
	u64 off_t[MM_TIMESTAMP_NUM]; /* ms */
	atomic_t user_ref_cnt[MM_TYPE_NR];
	u64 user_on_t[MM_TYPE_NR]; /* ms */
	u64 user_off_t[MM_TYPE_NR]; /* ms */
	u64 total_on_t; /* mbrain */
	u32 total_on_cnt;	/* mbrain */
	u64 user_total_on_t[MM_TYPE_NR]; /* mbrain */
	u32 user_total_on_cnt[MM_TYPE_NR]; /* mbrain */
	u64 max_latency; /* mbrain */
	u64 max_latency_t; /* mbrain */
};

struct mtk_mminfra_pd {
	u32 mminfra_mtcmos_num;
	u32 mminfra_type_num;
	u32 mminfra_type0_pwr;
	struct mminfra_mtcmos mm_mtcmos[MM_PWR_NUM_NR];
};

struct mtk_mminfra_util {
	void __iomem *vlp_base;
	u32 vlp_rsvd6_ofs;
	u32 irq_rdy_bit;
};

static struct device *g_dev;
static struct mtk_mminfra_pd *g_mm_pd;
static struct mtk_mminfra_util *g_mminfra_util;
spinlock_t mminfra_pd_lock;
static bool is_mminfra_util_shutdown;

void mtk_mminfra_voter_debug(u32 mm_pwr)
{
	/*not implement yet*/
}

#if IS_ENABLED(CONFIG_MTK_HWCCF)
int mminfra0_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_0].voter,
			HWCCF_VOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_0].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_0].voter,
			HWCCF_UNVOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_0].vote_bit);
	}

	return ret;
}

int mminfra1_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_1].voter,
			HWCCF_VOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_1].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_1].voter,
			HWCCF_UNVOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_1].vote_bit);
	}

	return ret;
}

int mminfra_ao_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_AO].voter,
			HWCCF_VOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_AO].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_AO].voter,
			HWCCF_UNVOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_AO].vote_bit);
	}

	return ret;
}

int mminfra2_onoff(bool on_off)
{
	int ret = 0;

	if (on_off) {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_2].voter,
			HWCCF_VOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_2].vote_bit);
	} else {
		ret = hwccf_irq_voter_ctrl(MM_HWCCF, g_mm_pd->mm_mtcmos[MM_PWR_MM_2].voter,
			HWCCF_UNVOTE, g_mm_pd->mm_mtcmos[MM_PWR_MM_2].vote_bit);
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_MTK_MMINFRA)
bool mtk_mminfra_is_on(u32 mm_pwr)
{
	if (mm_pwr >= MM_PWR_NUM_NR) {
		pr_notice("%s: power(%d) is invalid.\n",__func__, mm_pwr);
		return false;
	}
	if (!g_mm_pd->mm_mtcmos[mm_pwr].base) {
		pr_notice("%s: mm_mtcmos[%d].base is invalid.\n",__func__, mm_pwr);
		return true;
	}
	if (!g_mm_pd->mm_mtcmos[mm_pwr].mask) {
		pr_notice("%s: mm_mtcmos[%d].mask is invalid.\n",__func__, mm_pwr);
		return true;
	}
	if ((readl(g_mm_pd->mm_mtcmos[mm_pwr].base) & g_mm_pd->mm_mtcmos[mm_pwr].mask)
				!= g_mm_pd->mm_mtcmos[mm_pwr].mask) {
		pr_notice("%s: mm_mtcmos[%d] is off.\n",__func__, mm_pwr);
		return false;
	}
	return true;

}
EXPORT_SYMBOL_GPL(mtk_mminfra_is_on);

void mtk_mminfra_power_debug(u32 mm_pwr)
{
	int i;
	u64 time_ms = 0;

	// check mminfra_pd valid
	if (!g_mm_pd) {
		pr_notice("%s: not supported\n", __func__);
		return;
	}

	pr_notice("%s:pwr[%d] in.\n", __func__, mm_pwr);

	// check mm_pwr valid
	if (mm_pwr >= g_mm_pd->mminfra_mtcmos_num) {
		pr_notice("%s: invalid mm_pwr(%d)\n", __func__, mm_pwr);
		return;
	}

	pr_notice("%s:pwr[%d] timestamp:\n", __func__, mm_pwr);
	for (i = 0; i < MM_TIMESTAMP_NUM; i++) {
		if (atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].ref_cnt) > 0) /* power on */
			pr_notice("%s:pwr[%d] T[%d] off_t[%llu]ms on_t[%llu]ms.\n", __func__,
				mm_pwr, i,
				g_mm_pd->mm_mtcmos[mm_pwr].off_t[i],
				g_mm_pd->mm_mtcmos[mm_pwr].on_t[i]);
		else /* power off */
			pr_notice("%s:pwr[%d] T[%d] on_t[%llu]ms off_t[%llu]ms.\n", __func__,
				mm_pwr, i,
				g_mm_pd->mm_mtcmos[mm_pwr].on_t[i],
				g_mm_pd->mm_mtcmos[mm_pwr].off_t[i]);
	}

	pr_notice("%s:pwr[%d] user info:\n", __func__, mm_pwr);
	for (i = 0; i < g_mm_pd->mminfra_type_num; i++) {
		if (!g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_cnt[i])
			continue;
		pr_notice("%s:pwr[%d],user refcnt[%d]=[%d] on_t[%d]=[%llu]ms off_t[%d]=[%llu]ms.\n",
			__func__, mm_pwr,
			i, atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[i]),
			i, g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[i],
			i, g_mm_pd->mm_mtcmos[mm_pwr].user_off_t[i]);
	}

	pr_notice("%s:pwr[%d] mbrain info:\n", __func__, mm_pwr);
	time_ms = g_mm_pd->mm_mtcmos[mm_pwr].total_on_t;
	if (atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].ref_cnt) > 0)/* power on */
		time_ms += ((sched_clock()/1000000)
				- g_mm_pd->mm_mtcmos[mm_pwr].on_t[MM_TIMESTAMP_NUM - 1]);
	pr_notice("%s:pwr[%d],total_on_cnt=[%d] total_on_t=[%llu]ms.\n", __func__, mm_pwr,
		g_mm_pd->mm_mtcmos[mm_pwr].total_on_cnt, time_ms);
	for (i = 0; i < g_mm_pd->mminfra_type_num; i++) {
		if (!g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_cnt[i])
			continue;
		time_ms = g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_t[i];
		if (atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[i]) > 0)/* power on */
			time_ms += ((sched_clock()/1000000)
				- g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[i]);
		pr_notice("%s:pwr[%d],total_on_cnt[%d]=[%d] total_on_t[%d]=[%llu]ms.\n",
			__func__, mm_pwr,
			i, g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_cnt[i],
			i, time_ms);
	}
	pr_notice("%s:pwr[%d] max_latency=[%llu]ms in [%llu]ms.\n", __func__,
		mm_pwr, g_mm_pd->mm_mtcmos[mm_pwr].max_latency,
		g_mm_pd->mm_mtcmos[mm_pwr].max_latency_t);

	pr_notice("%s:pwr[%d] mtk_mminfra_is_on=[%d].\n", __func__,
		mm_pwr, mtk_mminfra_is_on(mm_pwr));
	mtk_mminfra_voter_debug(mm_pwr);

	pr_notice("%s:pwr[%d] out.\n", __func__, mm_pwr);
}
EXPORT_SYMBOL_GPL(mtk_mminfra_power_debug);

void mtk_mminfra_all_power_debug(void)
{
	u32 mm_pwr;

	for (mm_pwr = 0; mm_pwr < MM_PWR_NUM_NR ; mm_pwr++)
		mtk_mminfra_power_debug(mm_pwr);
}
EXPORT_SYMBOL_GPL(mtk_mminfra_all_power_debug);

int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type)
{
	int ret = 0, ref_cnt, user_ref_cnt, i, temp_cnt;
	unsigned long flags;
	u64 start_t_ms = sched_clock()/1000000;
	u64 end_t_ms = 0;
	u64 temp_t;

	// check mminfra_pd valid
	if (!g_mm_pd) {
		pr_notice("%s: not supported\n", __func__);
		return -EINVAL;
	}

	if (is_mminfra_util_shutdown) {
		pr_notice("%s:[err] bypass power[%d] on[%d], mm_type[%d] shutdown[%d].\n",
			__func__, mm_pwr, on_off, mm_type,
			is_mminfra_util_shutdown);
		WARN_ON(1);
		return -EINVAL;
	}

	spin_lock_irqsave(&mminfra_pd_lock, flags);

	// check mm_pwr valid
	if (mm_pwr >= g_mm_pd->mminfra_mtcmos_num) {
		pr_notice("%s: invalid mm_pwr(%d)\n", __func__, mm_pwr);
		spin_unlock_irqrestore(&mminfra_pd_lock, flags);
		return -EINVAL;
	}

	// check mm_type valid
	if (mm_type >= g_mm_pd->mminfra_type_num) {
		pr_notice("%s: invalid mm_type(%d)\n", __func__, mm_type);
		spin_unlock_irqrestore(&mminfra_pd_lock, flags);
		return -EINVAL;
	}

	if (on_off) {
		// add ref_cnt
		ref_cnt = atomic_inc_return(&g_mm_pd->mm_mtcmos[mm_pwr].ref_cnt);

		// add user ref_cnt
		user_ref_cnt
			= atomic_inc_return(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[mm_type]);

		if (ref_cnt != 1) {
			// pr_notice("%s: already enabled, mm_pwr(%d) ref_cnt(%d)\n",
			// 	__func__, mm_pwr, ref_cnt);
			if (user_ref_cnt == 1) {
				end_t_ms = sched_clock()/1000000;
				g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[mm_type] = end_t_ms;
				g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_cnt[mm_type]++;
			}
			spin_unlock_irqrestore(&mminfra_pd_lock, flags);
			return 0;
		}
#if IS_ENABLED(CONFIG_MTK_HWCCF)
		// power on mminfra
		if (mm_pwr == MM_PWR_MM_0)
			ret = mminfra0_onoff(true);
		else if (mm_pwr == MM_PWR_MM_1)
			ret = mminfra1_onoff(true);
		else if (mm_pwr == MM_PWR_MM_AO)
			ret = mminfra_ao_onoff(true);
		else if (mm_pwr == MM_PWR_MM_2)
			ret = mminfra2_onoff(true);
#endif
	} else {
		// minus ref_cnt
		ref_cnt = atomic_dec_return(&g_mm_pd->mm_mtcmos[mm_pwr].ref_cnt);

		// minus user ref_cnt
		if (atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[mm_type]) <= 0) {
			temp_cnt = atomic_read(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[mm_type]);
			pr_notice("%s:[err] mm_mtcmos[%d].user_ref_cnt[%d]=[%d] underflow.\n",
				__func__, mm_pwr, mm_type,
				temp_cnt);
				BUG_ON(1);
		}
		user_ref_cnt
			= atomic_dec_return(&g_mm_pd->mm_mtcmos[mm_pwr].user_ref_cnt[mm_type]);

		if (ref_cnt != 0) {
			// pr_notice("%s: ref_cnt>1, mm_pwr(%d) ref_cnt(%d)\n",
			// 	__func__, mm_pwr, ref_cnt);
			if (user_ref_cnt == 0) {
				end_t_ms = sched_clock()/1000000;
				g_mm_pd->mm_mtcmos[mm_pwr].user_off_t[mm_type] = end_t_ms;
				temp_t = end_t_ms - g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[mm_type];
				g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_t[mm_type] += temp_t;
			}
			spin_unlock_irqrestore(&mminfra_pd_lock, flags);
			return 0;
		}
#if IS_ENABLED(CONFIG_MTK_HWCCF)
		// power off mminfra
		if (mm_pwr == MM_PWR_MM_0)
			ret = mminfra0_onoff(false);
		else if (mm_pwr == MM_PWR_MM_1)
			ret = mminfra1_onoff(false);
		else if (mm_pwr == MM_PWR_MM_AO)
			ret = mminfra_ao_onoff(false);
		else if (mm_pwr == MM_PWR_MM_2)
			ret = mminfra2_onoff(false);
#endif
	}

	if (ret) {
		pr_notice("%s:[err] power(%d) on_off(%d) fail, type(%d) ret(%d).\n",
			__func__, mm_pwr, on_off, mm_type, ret);
		clkchk_external_dump();
		mtk_mminfra_voter_debug(mm_type);
		BUG_ON(1);
	}

	/* check power on */
	if (on_off && !mtk_mminfra_is_on(mm_pwr)) {
		pr_notice("%s:[err] power(%d) on_off(%d) fail, pwr is off, mm_type[%d].\n",
			__func__, mm_pwr, on_off, mm_type);
		mtk_mminfra_voter_debug(mm_type);
	}

	end_t_ms = sched_clock()/1000000;
	spin_unlock_irqrestore(&mminfra_pd_lock, flags);

	/* record timestamp */
	if (on_off) {
		for (i = 0; i < (MM_TIMESTAMP_NUM - 1); i++)
			g_mm_pd->mm_mtcmos[mm_pwr].on_t[i]
				= g_mm_pd->mm_mtcmos[mm_pwr].on_t[i+1];
		g_mm_pd->mm_mtcmos[mm_pwr].on_t[MM_TIMESTAMP_NUM - 1] = end_t_ms;
		g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[mm_type] = end_t_ms;
		g_mm_pd->mm_mtcmos[mm_pwr].total_on_cnt++;
		g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_cnt[mm_type]++;
	} else {
		for (i = 0; i < (MM_TIMESTAMP_NUM - 1); i++)
			g_mm_pd->mm_mtcmos[mm_pwr].off_t[i]
				= g_mm_pd->mm_mtcmos[mm_pwr].off_t[i+1];
		g_mm_pd->mm_mtcmos[mm_pwr].off_t[MM_TIMESTAMP_NUM - 1] = end_t_ms;
		g_mm_pd->mm_mtcmos[mm_pwr].user_off_t[mm_type] = end_t_ms;
		g_mm_pd->mm_mtcmos[mm_pwr].total_on_t += (end_t_ms
			- g_mm_pd->mm_mtcmos[mm_pwr].on_t[MM_TIMESTAMP_NUM - 1]);
		g_mm_pd->mm_mtcmos[mm_pwr].user_total_on_t[mm_type]
			+= (end_t_ms - g_mm_pd->mm_mtcmos[mm_pwr].user_on_t[mm_type]);
	}
	if ((end_t_ms - start_t_ms) > g_mm_pd->mm_mtcmos[mm_pwr].max_latency) {
		g_mm_pd->mm_mtcmos[mm_pwr].max_latency = end_t_ms - start_t_ms;
		g_mm_pd->mm_mtcmos[mm_pwr].max_latency_t = end_t_ms;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mminfra_on_off);
#endif

int mminfra_ctrl(struct cb_params *cb_para)
{
	//pr_notice("%s: name(%s) on_off(%d) vote_bit(%d)\n", __func__,
	//	cb_para->name, cb_para->onoff, cb_para->vote_bit);

	mtk_mminfra_on_off((cb_para->onoff ? true : false),
		g_mm_pd->mminfra_type0_pwr,
		MM_TYPE_CG_LINK);

	return 0;
}

static void mminfra_util_shutdown(struct platform_device *pdev)
{
	is_mminfra_util_shutdown = true;

	if (g_mminfra_util->vlp_base) {
		writel(readl(g_mminfra_util->vlp_base + g_mminfra_util->vlp_rsvd6_ofs) &
			~(1 << g_mminfra_util->irq_rdy_bit),
			g_mminfra_util->vlp_base + g_mminfra_util->vlp_rsvd6_ofs);

		pr_notice("[mminfra]%s shutdown done vlp:%x\n", __func__,
			readl(g_mminfra_util->vlp_base + g_mminfra_util->vlp_rsvd6_ofs));
	}
	pr_notice("[mminfra]%s shutdown\n", __func__);
}

static int mminfra_util_probe(struct platform_device *pdev)
{
	u32 i, j, tmp = 0, vlp_base_pa;

	g_mm_pd = devm_kzalloc(&pdev->dev, sizeof(*g_mm_pd), GFP_KERNEL);
	if (!g_mm_pd)
		return -ENOMEM;

	g_mminfra_util = devm_kzalloc(&pdev->dev, sizeof(*g_mminfra_util), GFP_KERNEL);
	if (!g_mminfra_util)
		return -ENOMEM;

	g_dev = &pdev->dev;

	of_property_read_u32(g_dev->of_node, "mminfra-mtcmos-num",
		&g_mm_pd->mminfra_mtcmos_num);
	of_property_read_u32(g_dev->of_node, "mminfra-type-num",
		&g_mm_pd->mminfra_type_num);
	of_property_read_u32(g_dev->of_node, "mminfra-type0-pwr",
		&g_mm_pd->mminfra_type0_pwr);
	for (i = 0; i < MM_PWR_NUM_NR; i++) {
		if (i >= g_mm_pd->mminfra_mtcmos_num)
			break;
		if (!of_property_read_u32_index(g_dev->of_node, "mminfra-mtcmos-voter", i, &tmp)) {
			g_mm_pd->mm_mtcmos[i].voter = tmp;
			pr_notice("[mminfra] mm[%d] voter(%d)\n",
				i, g_mm_pd->mm_mtcmos[i].voter);
		}
		if (!of_property_read_u32_index(g_dev->of_node, "mminfra-mtcmos-data", i, &tmp)) {
			g_mm_pd->mm_mtcmos[i].vote_bit = tmp;
			atomic_set(&g_mm_pd->mm_mtcmos[i].ref_cnt, 0);
			for (j = 0; j < MM_TYPE_NR; j++)
				atomic_set(&g_mm_pd->mm_mtcmos[i].user_ref_cnt[j], 0);
			pr_notice("[mminfra] mm[%d] vote_bit(%d)\n",
				i, g_mm_pd->mm_mtcmos[i].vote_bit);
		}
		if (!of_property_read_u32_index(g_dev->of_node, "mm-mtcmos-base", i, &tmp)) {
			if (tmp)
				g_mm_pd->mm_mtcmos[i].base = ioremap(tmp, 0x4);
			pr_notice("[mminfra] mm[%d] mm-mtcmos-base(0x%x)\n", i, tmp);
		}
		if (!of_property_read_u32_index(g_dev->of_node, "mm-mtcmos-mask", i, &tmp)) {
			if (tmp)
				g_mm_pd->mm_mtcmos[i].mask = tmp;
			pr_notice("[mminfra] mm[%d] mm-mtcmos-mask(0x%x)\n",
				i, g_mm_pd->mm_mtcmos[i].mask);
		}
	}

	if (!of_property_read_u32(g_dev->of_node, "mminfra-vlp-base", &vlp_base_pa))
		g_mminfra_util->vlp_base = ioremap(vlp_base_pa, 0x1000);
	else
		g_mminfra_util->vlp_base = NULL;

	of_property_read_u32(g_dev->of_node, "mminfra-vlp-ao-rsvd6-ofs", &(g_mminfra_util->vlp_rsvd6_ofs));
	of_property_read_u32(g_dev->of_node, "mminfra-mm1-irq-rdy-bit", &(g_mminfra_util->irq_rdy_bit));

	spin_lock_init(&mminfra_pd_lock);
#if IS_ENABLED(CONFIG_MTK_HWCCF)
	int ret = register_mtk_clk_external_api_cb(CLK_REQUEST_MMINFRA_CB, &mminfra_ctrl, NULL);
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
	.shutdown = mminfra_util_shutdown,
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
MODULE_LICENSE("GPL");
