// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6377/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "mtk_battery_oc_throttling.h"

#define MT6375_FGADC_CUR_CON1	0x2E9
#define MT6375_FGADC_CUR_CON2	0x2EB
#define MT6375_FGADC_ANA_ELR4	0x263
#define FG_GAINERR_SEL_MASK	GENMASK(1, 0)

/* Customize the setting in dts node */
#define DEF_BAT_OC_THD_H	6800
#define DEF_BAT_OC_THD_L	8000

#define UNIT_TRANS_10		10
#define CURRENT_CONVERT_RATIO	95
#define OCCB_MAX_NUM		16

/* Get r-fg-value/car-tune-value from gauge dts node */
#define	MT6357_DEFAULT_RFG		(100)
#define	MT6357_UNIT_FGCURRENT		(314331)

#define	MT6358_DEFAULT_RFG		(100)
#define	MT6358_UNIT_FGCURRENT		(381470)

#define	MT6359P_DEFAULT_RFG		(50)
#define	MT6359P_UNIT_FGCURRENT		(610352)

#define	MT6375_UNIT_FGCURRENT		(610352)

#define MT6377_CHIP_ID			(0x77)
#define	MT6377_DEFAULT_RFG		(50)
#define	MT6377_UNIT_FGCURRENT		(610352)

#define MTK_BATOC_DIR_NAME		"mtk_batoc_throttling"
#define DEFAULT_BUF_LEN			512
#define PMIC_SPMI_SWCID			(0xB)

struct reg_t {
	unsigned int addr;
	unsigned int mask;
	size_t size;
};

struct battery_oc_data_t {
	const char *regmap_source;
	const char *gauge_node_name;
	struct reg_t fg_cur_hth;
	struct reg_t fg_cur_lth;
	bool spmi_intf;
	bool cust_rfg;
	struct reg_t reg_default_rfg;
};

struct battery_oc_data_t mt6359p_battery_oc_data = {
	.regmap_source = "parent_drvdata",
	.gauge_node_name = "mtk-gauge",
	.fg_cur_hth = {MT6359P_FGADC_CUR_CON2, 0xFFFF, 1},
	.fg_cur_lth = {MT6359P_FGADC_CUR_CON1, 0xFFFF, 1},
	.spmi_intf = false,
	.cust_rfg = false,
};

struct battery_oc_data_t mt6375_battery_oc_data = {
	.regmap_source = "dev_get_regmap",
	.gauge_node_name = "mtk-gauge",
	.fg_cur_hth = {MT6375_FGADC_CUR_CON2, 0xFFFF, 2},
	.fg_cur_lth = {MT6375_FGADC_CUR_CON1, 0xFFFF, 2},
	.spmi_intf = false,
	.cust_rfg = true,
	.reg_default_rfg = {MT6375_FGADC_ANA_ELR4, FG_GAINERR_SEL_MASK, 1},
};

struct battery_oc_data_t mt6377_battery_oc_data = {
	.regmap_source = "dev_get_regmap",
	.gauge_node_name = "mtk-gauge",
	.fg_cur_hth = {MT6377_FGADC_CUR_CON2_L, 0xFFFF, 2},
	.fg_cur_lth = {MT6377_FGADC_CUR_CON1_L, 0xFFFF, 2},
	.spmi_intf = true,
	.cust_rfg = false,
};

struct battery_oc_priv {
	struct device *dev;
	struct regmap *regmap;
	unsigned int oc_level;
	unsigned int oc_thd_h[BATTERY_OC_LEVEL_NUM];
	unsigned int oc_thd_l[BATTERY_OC_LEVEL_NUM];
	unsigned int oc_enb_h[BATTERY_OC_LEVEL_NUM];
	unsigned int oc_enb_l[BATTERY_OC_LEVEL_NUM];
	int fg_cur_h_irq;
	int fg_cur_l_irq;
	int r_fg_value;
	int default_rfg;
	int car_tune_value;
	int unit_fg_cur;
	int unit_multiple;
	const struct battery_oc_data_t *ocdata;
	int ppb_mode;
};

struct battery_oc_callback_table {
	void (*occb)(enum BATTERY_OC_LEVEL_TAG, void *data);
	void *data;
};

static struct battery_oc_callback_table occb_tb[OCCB_MAX_NUM] = { {0}, {0} };
static int g_battery_oc_stop;
static struct battery_oc_priv *bat_oc_data;

static int __regmap_update_bits(struct regmap *regmap, const struct reg_t *reg,
				unsigned int val)
{
	if (reg->size == 1)
		return regmap_update_bits(regmap, reg->addr, reg->mask, val);
	/*
	 * here we assume those register addresses are continuous and
	 * there is one and only one function in them.
	 * please take care of the endian if it is necessary.
	 * this is not a good assumption but we do this here for compatibility.
	 * please abstract the register control if there is a chance to refactor
	 * this file.
	 */
	val &= reg->mask;
	return regmap_bulk_write(regmap, reg->addr, &val, reg->size);
}

static int __regmap_read(struct regmap *regmap, const struct reg_t *reg,
			 unsigned int *val)
{
	if (reg->size == 1)
		return regmap_read(regmap, reg->addr, val);
	return regmap_bulk_read(regmap, reg->addr, val, reg->size);
}

void register_battery_oc_notify(battery_oc_callback oc_cb,
				enum BATTERY_OC_PRIO_TAG prio_val, void *data)
{
	if (prio_val >= OCCB_MAX_NUM) {
		pr_info("[%s] prio_val=%d, out of boundary\n",
			__func__, prio_val);
		return;
	}
	occb_tb[prio_val].occb = oc_cb;
	occb_tb[prio_val].data = data;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
}
EXPORT_SYMBOL(register_battery_oc_notify);

void exec_battery_oc_callback(enum BATTERY_OC_LEVEL_TAG battery_oc_level)
{
	int i;

	if (g_battery_oc_stop == 1) {
		pr_info("[%s] g_battery_oc_stop=%d\n"
			, __func__, g_battery_oc_stop);
	} else {
		for (i = 0; i < OCCB_MAX_NUM; i++) {
			if (occb_tb[i].occb)
				occb_tb[i].occb(battery_oc_level, occb_tb[i].data);
		}
		pr_info("[%s] battery_oc_level=%d\n", __func__, battery_oc_level);
	}
}

static int battery_oc_throttling_open(struct inode *inode, struct file *fp)
{
	fp->private_data = pde_data(inode);
	return 0;
}

static ssize_t battery_oc_protect_ut_read(struct file *fp, char __user *ubuf,
					  size_t cnt, loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;
	char *buf;
	u32 len;
	ssize_t ret;

	pr_debug("[%s] g_battery_oc_level=%d\n", __func__, priv->oc_level);

	buf = kzalloc(DEFAULT_BUF_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf, DEFAULT_BUF_LEN, "%u\n", priv->oc_level);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t battery_oc_protect_ut_write(struct file *fp,
					   const char __user *ubuf, size_t cnt,
					   loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;
	char *buf, cmd[DEFAULT_BUF_LEN + 1];
	unsigned int val = 0;
	ssize_t ret;

	buf = kzalloc(DEFAULT_BUF_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = simple_write_to_buffer(buf, DEFAULT_BUF_LEN, ppos, ubuf, cnt);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(priv->dev, "parameter number not correct\n");
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val < BATTERY_OC_LEVEL_NUM) {
		dev_info(priv->dev, "[%s] your input is %d\n", __func__, val);
		exec_battery_oc_callback(val);
	} else {
		dev_info(priv->dev, "[%s] wrong number (%d)\n", __func__, val);
	}

	return cnt;
}

static ssize_t battery_oc_protect_stop_read(struct file *fp, char __user *ubuf,
					    size_t cnt, loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;
	char *buf;
	u32 len;
	ssize_t ret;

	dev_dbg(priv->dev, "[%s] g_battery_oc_stop=%d\n", __func__, g_battery_oc_stop);

	buf = kzalloc(DEFAULT_BUF_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf, DEFAULT_BUF_LEN, "%u\n", g_battery_oc_stop);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t battery_oc_protect_stop_write(struct file *fp,
					     const char __user *ubuf,
					     size_t cnt, loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;
	char *buf, cmd[DEFAULT_BUF_LEN + 1];
	unsigned int val = 0;
	ssize_t ret;

	buf = kzalloc(DEFAULT_BUF_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = simple_write_to_buffer(buf, DEFAULT_BUF_LEN, ppos, ubuf, cnt);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(priv->dev, "parameter number not correct\n");
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1))
		val = 0;

	g_battery_oc_stop = val;
	dev_info(priv->dev, "g_battery_oc_stop=%d\n", g_battery_oc_stop);

	return cnt;
}

static ssize_t battery_oc_protect_level_read(struct file *fp, char __user *ubuf,
					     size_t cnt, loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;
	char *buf;
	u32 len;
	ssize_t ret;

	dev_info(priv->dev, "[%s] g_battery_oc_level=%d\n", __func__,
		 priv->oc_level);

	buf = kzalloc(DEFAULT_BUF_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf, DEFAULT_BUF_LEN, "%d\n", priv->oc_level);

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t battery_oc_protect_level_write(struct file *fp,
					      const char __user *ubuf,
					      size_t cnt, loff_t *ppos)
{
	struct battery_oc_priv *priv = fp->private_data;

	dev_info(priv->dev, "[%s] g_battery_oc_level = %d\n", __func__,
		 priv->oc_level);

	return cnt;
}

#define BATOC_THROTTLING_OPS(_name) \
struct proc_ops _name##_fops = { \
	.proc_open = battery_oc_throttling_open, \
	.proc_read = _name##_read, \
	.proc_write = _name##_write, \
}

static const BATOC_THROTTLING_OPS(battery_oc_protect_ut);
static const BATOC_THROTTLING_OPS(battery_oc_protect_stop);
static const BATOC_THROTTLING_OPS(battery_oc_protect_level);

/*
 * 65535 - (I_mA * 1000 * r_fg_value / DEFAULT_RFG * 1000000 / car_tune_value
 * / UNIT_FGCURRENT * CURRENT_CONVERT_RATIO / 100)
 */
static unsigned int to_fg_code(struct battery_oc_priv *priv, u64 cur_mA)
{
	cur_mA = div_u64(cur_mA * 1000 * priv->r_fg_value, priv->default_rfg);
	cur_mA = div_u64(cur_mA * 1000000, priv->car_tune_value);
	cur_mA = div_u64(cur_mA, priv->unit_fg_cur);
	cur_mA = div_u64(cur_mA * CURRENT_CONVERT_RATIO, 100);

	/* 2's complement */
	return (0xFFFF - cur_mA);
}

static void switch_bat_oc_level(struct battery_oc_priv *priv, int step)
{
	if (step && priv->oc_enb_h[priv->oc_level])
		disable_irq_nosync(priv->fg_cur_h_irq);

	if (step && priv->oc_enb_l[priv->oc_level])
		disable_irq_nosync(priv->fg_cur_l_irq);

	// update current level
	priv->oc_level = priv->oc_level + step;
	exec_battery_oc_callback(priv->oc_level);

	// config new battery current threshold
	__regmap_update_bits(priv->regmap, &priv->ocdata->fg_cur_hth,
		to_fg_code(priv, priv->oc_thd_h[priv->oc_level]));
	__regmap_update_bits(priv->regmap, &priv->ocdata->fg_cur_lth,
		to_fg_code(priv, priv->oc_thd_l[priv->oc_level]));

	// set property battery current interrupt
	if (priv->oc_enb_l[priv->oc_level])
		enable_irq(priv->fg_cur_l_irq);

	if (priv->oc_enb_h[priv->oc_level])
		enable_irq(priv->fg_cur_h_irq);
}

static irqreturn_t fg_cur_h_int_handler(int irq, void *data)
{
	struct battery_oc_priv *priv = data;

	if (priv->oc_level >= BATTERY_OC_LEVEL_NUM || priv->oc_level < BATTERY_OC_LEVEL_1
		 || priv->ppb_mode != 0) {
		pr_info("%s: wrong oc_level=%d, ppb_mode=%d\n", __func__, priv->oc_level,
			priv->ppb_mode);
		return IRQ_HANDLED;
	}

	switch_bat_oc_level(priv, -1);

	return IRQ_HANDLED;
}

static irqreturn_t fg_cur_l_int_handler(int irq, void *data)
{
	struct battery_oc_priv *priv = data;

	// filter wrong level
	if (priv->oc_level > BATTERY_OC_LEVEL_NUM - 2 || priv->ppb_mode != 0) {
		pr_info("%s: wrong oc_level=%d, ppb=%d\n", __func__, priv->oc_level,
			priv->ppb_mode);
		return IRQ_HANDLED;
	}

	switch_bat_oc_level(priv, 1);

	return IRQ_HANDLED;
}

static int battery_oc_parse_dt(struct platform_device *pdev)
{
	struct battery_oc_priv *priv = dev_get_drvdata(&pdev->dev);
	struct mt6397_chip *pmic;
	struct device_node *np;
	const int r_fg_val[] = { 50, 20, 10, 5 };
	int i, ret = 0, oc_thd_size = 0;
	unsigned int *oc_thd;
	u32 regval = 0;


	/* Get r-fg-value/car-tune-value from gauge dts node */
	np = of_find_node_by_name(pdev->dev.parent->of_node,
				  priv->ocdata->gauge_node_name);
	if (!np) {
		dev_notice(&pdev->dev, "get mtk-gauge node fail\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "r-fg-value", &priv->r_fg_value);
	if (ret) {
		dev_notice(&pdev->dev, "get r-fg-value fail\n");
		return -EINVAL;
	}
	priv->r_fg_value *= UNIT_TRANS_10;

	ret = of_property_read_u32(np, "unit-multiple", &priv->unit_multiple);
	if (ret) {
		dev_notice(&pdev->dev, "get unit-multiple fail\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "car-tune-value", &priv->car_tune_value);
	if (ret) {
		dev_notice(&pdev->dev, "get car-tune-value fail\n");
		return -EINVAL;
	}
	priv->car_tune_value *= UNIT_TRANS_10;

	/*
	 * Get oc_thd_h/oc_thd_l value from dts node.
	 * For compatibility, there are 2 possible naming,
	 * one is "mtk_battery_oc_throttling", and the other is
	 * "mtk-battery-oc-throttling".
	 */
	np = of_find_node_by_name(pdev->dev.parent->of_node,
				  "mtk_battery_oc_throttling");
	if (!np)
		np = of_find_node_by_name(pdev->dev.parent->of_node,
					  "mtk-battery-oc-throttling");
	if (!np) {
		dev_notice(&pdev->dev, "get mtk battery oc node fail\n");
		return -EINVAL;
	}

	oc_thd_size = of_property_count_elems_of_size(np, "oc-thd", sizeof(u32));

	if (oc_thd_size == BATTERY_OC_LEVEL_NUM) {
		oc_thd = devm_kmalloc_array(&pdev->dev, oc_thd_size, sizeof(u32), GFP_KERNEL);
		ret = of_property_read_u32_array(np, "oc-thd", oc_thd, oc_thd_size);
		if (ret) {
			dev_notice(&pdev->dev, "get oc-thd fail\n");
			return -EINVAL;
		}

		/* init level_0 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_0] = 1;
		priv->oc_enb_h[BATTERY_OC_LEVEL_0] = 0;
		priv->oc_thd_l[BATTERY_OC_LEVEL_0] = oc_thd[1];
		priv->oc_thd_h[BATTERY_OC_LEVEL_0] = 0;

		/* init level_1 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_1] = 1;
		priv->oc_enb_h[BATTERY_OC_LEVEL_1] = 1;
		priv->oc_thd_l[BATTERY_OC_LEVEL_1] = oc_thd[2];
		priv->oc_thd_h[BATTERY_OC_LEVEL_1] = oc_thd[0];

		/* init level_2 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_2] = 0;
		priv->oc_enb_h[BATTERY_OC_LEVEL_2] = 1;
		priv->oc_thd_l[BATTERY_OC_LEVEL_2] = 0;
		priv->oc_thd_h[BATTERY_OC_LEVEL_2] = oc_thd[1];

	} else {
		oc_thd = devm_kmalloc_array(&pdev->dev,
					BATTERY_OC_LEVEL_NUM, sizeof(u32), GFP_KERNEL);
		ret = of_property_read_u32(np, "oc-thd-h", &oc_thd[0]);
		ret |= of_property_read_u32(np, "oc-thd-l", &oc_thd[1]);
		if (ret) {
			dev_info(&pdev->dev, "get threshold error, use default setting");
			oc_thd[0] = DEF_BAT_OC_THD_H;
			oc_thd[1] = DEF_BAT_OC_THD_L;
		}

		/* init level_0 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_0] = 1;
		priv->oc_enb_h[BATTERY_OC_LEVEL_0] = 0;
		priv->oc_thd_l[BATTERY_OC_LEVEL_0] = oc_thd[1];
		priv->oc_thd_h[BATTERY_OC_LEVEL_0] = 0;

		/* init level_1 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_1] = 0;
		priv->oc_enb_h[BATTERY_OC_LEVEL_1] = 1;
		priv->oc_thd_l[BATTERY_OC_LEVEL_1] = 0;
		priv->oc_thd_h[BATTERY_OC_LEVEL_1] = oc_thd[0];

		/* init level_2 oc table */
		priv->oc_enb_l[BATTERY_OC_LEVEL_2] = 0;
		priv->oc_enb_h[BATTERY_OC_LEVEL_2] = 0;
		priv->oc_thd_l[BATTERY_OC_LEVEL_2] = 0;
		priv->oc_thd_h[BATTERY_OC_LEVEL_2] = 0;
	}

	for (i = 0; i < BATTERY_OC_LEVEL_NUM; i++) {
		dev_notice(&pdev->dev, "[%s] intr_info[%d]: l[%d %d] h[%d %d]\n",
			__func__, i, priv->oc_enb_l[i], priv->oc_thd_l[i],
			priv->oc_enb_h[i], priv->oc_thd_h[i]);
	}

	/* Get DEFAULT_RFG/UNIT_FGCURRENT from pre-defined MACRO */
	if (priv->ocdata->cust_rfg) {
		__regmap_read(priv->regmap, &priv->ocdata->reg_default_rfg, &regval);
		regval &= priv->ocdata->reg_default_rfg.mask;
		/* The real rfg gain is r_fg_value * unit_multiple */
		if (priv->r_fg_value == 20 || priv->unit_multiple != 1)
			priv->default_rfg = priv->r_fg_value;
		else
			priv->default_rfg = r_fg_val[regval];
		priv->unit_fg_cur = MT6375_UNIT_FGCURRENT * priv->unit_multiple;
	} else if (priv->ocdata->spmi_intf) {
		ret = regmap_read(priv->regmap, PMIC_SPMI_SWCID, &regval);
		if (ret) {
			dev_info(&pdev->dev, "Failed to read chip id: %d\n", ret);
			return ret;
		}
		switch (regval) {
		case MT6377_CHIP_ID:
			priv->default_rfg = MT6377_DEFAULT_RFG;
			priv->unit_fg_cur = MT6377_UNIT_FGCURRENT;
			break;

		default:
			dev_info(&pdev->dev, "unsupported chip: 0x%x\n", regval);
			return -EINVAL;
		}
	} else {
		pmic = dev_get_drvdata(pdev->dev.parent);
		switch (pmic->chip_id) {
		case MT6359P_CHIP_ID:
			priv->default_rfg = MT6359P_DEFAULT_RFG;
			priv->unit_fg_cur = MT6359P_UNIT_FGCURRENT;
			break;

		default:
			dev_info(&pdev->dev, "unsupported chip: 0x%x\n", pmic->chip_id);
			return -EINVAL;
		}
	}
	dev_info(&pdev->dev, "r_fg=%d car_tune=%d DEFAULT_RFG=%d UNIT_FGCURRENT=%d\n"
		 , priv->r_fg_value, priv->car_tune_value
		 , priv->default_rfg, priv->unit_fg_cur);
	return 0;
}

static int battery_oc_throttling_create_proc(struct battery_oc_priv *priv)
{
#define ENTRY_DESC(_name) { #_name, &_name##_fops }
	const struct {
		const char *name;
		const struct proc_ops *fops;
	} entry_list[] = {
		ENTRY_DESC(battery_oc_protect_ut),
		ENTRY_DESC(battery_oc_protect_stop),
		ENTRY_DESC(battery_oc_protect_level)
	};
	struct proc_dir_entry *root_entry, *entry;
	int i;

	root_entry = proc_mkdir(MTK_BATOC_DIR_NAME, NULL);
	if (!root_entry) {
		dev_err(priv->dev, "Unable to craete proc dir\n");
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(entry_list); i++) {
		entry = proc_create_data(entry_list[i].name, 0644, root_entry,
					 entry_list[i].fops, priv);
		if (!entry)
			goto create_proc_fail;
	}

	return 0;

create_proc_fail:
	remove_proc_subtree(MTK_BATOC_DIR_NAME, NULL);
	return -ENODEV;
}

static int battery_oc_throttling_probe(struct platform_device *pdev)
{
	struct battery_oc_priv *priv;
	struct mt6397_chip *chip;
	int ret;

	pr_info("%s Jeff\n", __func__);
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, priv);
	priv->ocdata = of_device_get_match_data(&pdev->dev);
	if (!strcmp(priv->ocdata->regmap_source, "parent_drvdata")) {
		chip = dev_get_drvdata(pdev->dev.parent);
		priv->regmap = chip->regmap;
	} else
		priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);

	/* set Maximum threshold to avoid irq being triggered at init */
	__regmap_update_bits(priv->regmap, &priv->ocdata->fg_cur_hth, 0x7FFF);
	__regmap_update_bits(priv->regmap, &priv->ocdata->fg_cur_lth, 0x8000);
	priv->fg_cur_h_irq = platform_get_irq_byname(pdev, "fg_cur_h");
	if (priv->fg_cur_h_irq < 0) {
		dev_notice(&pdev->dev, "failed to get fg_cur_h irq, ret=%d\n",
			   priv->fg_cur_h_irq);
		return priv->fg_cur_h_irq;
	}

	priv->fg_cur_l_irq = platform_get_irq_byname(pdev, "fg_cur_l");
	if (priv->fg_cur_l_irq < 0) {
		dev_notice(&pdev->dev, "failed to get fg_cur_l irq, ret=%d\n",
			   priv->fg_cur_l_irq);
		return priv->fg_cur_l_irq;
	}

	/* Set IRQ NO AUTO enable */
	ret = devm_request_threaded_irq(&pdev->dev, priv->fg_cur_h_irq, NULL,
					fg_cur_h_int_handler, IRQF_ONESHOT | IRQF_NO_AUTOEN,
					"fg_cur_h", priv);
	if (ret < 0)
		dev_notice(&pdev->dev, "request fg_cur_h irq fail\n");
	ret = devm_request_threaded_irq(&pdev->dev, priv->fg_cur_l_irq, NULL,
					fg_cur_l_int_handler, IRQF_ONESHOT | IRQF_NO_AUTOEN,
					"fg_cur_l", priv);
	if (ret < 0)
		dev_notice(&pdev->dev, "request fg_cur_l irq fail\n");

	ret = battery_oc_parse_dt(pdev);
	if (ret < 0) {
		dev_notice(&pdev->dev, "bat_oc parse dt fail, ret=%d\n", ret);
		return ret;
	}

	switch_bat_oc_level(priv, 0);

	dev_info(&pdev->dev, "%dmA(0x%x), %dmA(0x%x) Done\n",
		 priv->oc_thd_h[priv->oc_level], to_fg_code(priv, priv->oc_thd_h[priv->oc_level]),
		 priv->oc_thd_l[priv->oc_level], to_fg_code(priv, priv->oc_thd_l[priv->oc_level]));

	bat_oc_data = priv;

	return battery_oc_throttling_create_proc(priv);
}

static int battery_oc_throttle_enable(struct battery_oc_priv *priv, int en)
{
	if (!en) {
		//disable all interrupt
		if (priv->oc_enb_l[priv->oc_level])
			disable_irq_nosync(priv->fg_cur_l_irq);
		if (priv->oc_enb_h[priv->oc_level])
			disable_irq_nosync(priv->fg_cur_h_irq);
	} else {
		//enable property interrupt
		if (priv->oc_enb_l[priv->oc_level])
			enable_irq(priv->fg_cur_l_irq);
		if (priv->oc_enb_h[priv->oc_level])
			enable_irq(priv->fg_cur_h_irq);
	}
	return 0;
}

int bat_oc_set_ppb_mode(unsigned int mode)
{
	struct battery_oc_priv *priv;

	if (!bat_oc_data) {
		pr_info("[%s] get battery oc data fail\n", __func__);
		return 0;
	}
	priv = bat_oc_data;
	priv->ppb_mode = mode;

	if (mode != 0) {
		battery_oc_throttle_enable(priv, 0);
		if (priv->oc_level) {
			priv->oc_level = 0;
			exec_battery_oc_callback(priv->oc_level);
		}
	} else
		switch_bat_oc_level(priv, 0);

	return 0;
}
EXPORT_SYMBOL(bat_oc_set_ppb_mode);

static int battery_oc_throtting_remove(struct platform_device *pdev)
{
	remove_proc_subtree(MTK_BATOC_DIR_NAME, NULL);
	return 0;
}

static int __maybe_unused battery_oc_throttling_suspend(struct device *d)
{
	struct battery_oc_priv *priv = dev_get_drvdata(d);

	//disable all interrupt
	if (priv->oc_enb_l[priv->oc_level])
		disable_irq_nosync(priv->fg_cur_l_irq);

	if (priv->oc_enb_h[priv->oc_level])
		disable_irq_nosync(priv->fg_cur_h_irq);

	return 0;
}

static int __maybe_unused battery_oc_throttling_resume(struct device *d)
{
	struct battery_oc_priv *priv = dev_get_drvdata(d);

	//enable property interrupt
	if (priv->oc_enb_l[priv->oc_level])
		enable_irq(priv->fg_cur_l_irq);

	if (priv->oc_enb_h[priv->oc_level])
		enable_irq(priv->fg_cur_h_irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(battery_oc_throttling_pm_ops,
			 battery_oc_throttling_suspend,
			 battery_oc_throttling_resume);

static const struct of_device_id battery_oc_throttling_of_match[] = {
	{
		.compatible = "mediatek,mt6359p-battery_oc_throttling",
		.data = &mt6359p_battery_oc_data,
	}, {
		.compatible = "mediatek,mt6375-battery_oc_throttling",
		.data = &mt6375_battery_oc_data,
	}, {
		.compatible = "mediatek,mt6377-battery_oc_throttling",
		.data = &mt6377_battery_oc_data,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, battery_oc_throttling_of_match);

static struct platform_driver battery_oc_throttling_driver = {
	.driver = {
		.name = "mtk_battery_oc_throttling",
		.of_match_table = battery_oc_throttling_of_match,
		.pm = &battery_oc_throttling_pm_ops,
	},
	.probe = battery_oc_throttling_probe,
	.remove = battery_oc_throtting_remove,
};
module_platform_driver(battery_oc_throttling_driver);

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MTK battery over current throttling driver");
MODULE_LICENSE("GPL");
