// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/export.h>
#include <linux/linear_range.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "pmic_lvsys_notify.h"

#define LVSYS_DBG		0
#define LVSYS_VIO18_SWITCH	0
#define LVSYS_INT_EN_SIZE	2
#define LVSYS_INT_FALLING_SIZE	2
#define LVSYS_INT_RISING_SIZE	2
#define LVSYS_EDGE_NUM		2

#define EVENT_LVSYS_F		0
#define EVENT_LVSYS_R		BIT(15)

#define MT6661_TMA_UNLOCK_VALUE 0x999E

struct pmic_lvsys_info {
	u32 hwcid;
	u32 lvsys_int_en_reg[LVSYS_INT_EN_SIZE];
	u32 lvsys_int_en_mask[LVSYS_INT_EN_SIZE];
	u32 lvsys_int_fdb_sel_reg[LVSYS_INT_FALLING_SIZE];
	u32 lvsys_int_fdb_sel_mask;
	u32 lvsys_int_rdb_sel_reg[LVSYS_INT_RISING_SIZE];
	u32 lvsys_int_rdb_sel_mask;
	u32 lvsys_int_vthl_reg[LVSYS_INT_FALLING_SIZE];
	u32 lvsys_int_vthl_mask[LVSYS_INT_FALLING_SIZE];
	u32 lvsys_int_vthh_reg[LVSYS_INT_RISING_SIZE];
	u32 lvsys_int_vthh_mask[LVSYS_INT_RISING_SIZE];
	u32 lvsys_int_o_sel_reg;
	u32 lvsys_int_o_sel_mask;
	u32 lvsys_int_o_sel_shift;
	u16 lvsys_int_notify[LVSYS_INT_FALLING_SIZE + LVSYS_INT_RISING_SIZE];
	u32 tma_key_reg;
	u16 tma_key_unlock_val;
	const struct linear_range vthl_range[LVSYS_INT_FALLING_SIZE];
	const struct linear_range vthh_range[LVSYS_INT_RISING_SIZE];
};

enum INT_NOTIFIY {
	GPIO,
	SPMI_RCS,
	NONE,
};

/*
 * Note:
 * 1. Append the number of 0 in the end to align macro num
 * 2. lvsys_int_notify order: LVSYS1_RISING, LVSYS1_FALLING,
 *			      LVSYS2_RISING, LVSYS2_FALLING
 */
static const struct pmic_lvsys_info mt6363_lvsys_info = {
	.hwcid = MT6363_HWCID1,
	.lvsys_int_en_reg = {MT6363_RG_LVSYS_INT_EN_ADDR, 0},
	.lvsys_int_en_mask = {MT6363_RG_LVSYS_INT_EN_MASK <<
			      MT6363_RG_LVSYS_INT_EN_SHIFT, 0},
	.lvsys_int_fdb_sel_reg = {MT6363_RG_LVSYS_INT_FDB_SEL_ADDR, 0},
	.lvsys_int_fdb_sel_mask = MT6363_RG_LVSYS_INT_FDB_SEL_MASK <<
				  MT6363_RG_LVSYS_INT_FDB_SEL_SHIFT,
	.lvsys_int_rdb_sel_reg = {MT6363_RG_LVSYS_INT_RDB_SEL_ADDR, 0},
	.lvsys_int_rdb_sel_mask = MT6363_RG_LVSYS_INT_RDB_SEL_MASK <<
				  MT6363_RG_LVSYS_INT_RDB_SEL_SHIFT,
	.lvsys_int_vthl_reg = {MT6363_RG_LVSYS_INT_VTHL_ADDR, 0},
	.lvsys_int_vthl_mask = {MT6363_RG_LVSYS_INT_VTHL_MASK <<
				MT6363_RG_LVSYS_INT_VTHL_SHIFT, 0},
	.lvsys_int_vthh_reg = {MT6363_RG_LVSYS_INT_VTHH_ADDR, 0},
	.lvsys_int_vthh_mask = {MT6363_RG_LVSYS_INT_VTHH_MASK <<
				MT6363_RG_LVSYS_INT_VTHH_SHIFT, 0},
	.lvsys_int_notify = {SPMI_RCS, SPMI_RCS, NONE, NONE},
	.vthl_range = {
		{
			.min = 2500,
			.min_sel = 0,
			.max_sel = 9,
			.step = 100,
		}, {
			.min = 0,
			.min_sel = 0,
			.max_sel = 0,
			.step = 0,
		},
	},
	.vthh_range = {
		{
			.min = 2600,
			.min_sel = 0,
			.max_sel = 9,
			.step = 100,
		}, {
			.min = 0,
			.min_sel = 0,
			.max_sel = 0,
			.step = 0,
		},
	},
};

static const struct pmic_lvsys_info mt6661_lvsys_info = {
	.hwcid = MT6661_HWCID1,
	.lvsys_int_en_reg = {MT6661_RG_LVSYS1_INT_EN_ADDR,
			     MT6661_RG_LVSYS2_INT_EN_ADDR},
	.lvsys_int_en_mask = {MT6661_RG_LVSYS1_INT_EN_MASK << MT6661_RG_LVSYS1_INT_EN_SHIFT,
			      MT6661_RG_LVSYS2_INT_EN_MASK << MT6661_RG_LVSYS2_INT_EN_SHIFT},
	.lvsys_int_fdb_sel_reg = {MT6661_RG_LVSYS_INT1_FALLING_DB_SEL_ADDR,
				  MT6661_RG_LVSYS_INT2_FALLING_DB_SEL_ADDR},
	.lvsys_int_fdb_sel_mask = MT6661_RG_LVSYS_INT1_FALLING_DB_SEL_MASK <<
				  MT6661_RG_LVSYS_INT1_FALLING_DB_SEL_SHIFT,
	.lvsys_int_rdb_sel_reg = {MT6661_RG_LVSYS_INT1_RISING_DB_SEL_ADDR,
				  MT6661_RG_LVSYS_INT2_RISING_DB_SEL_ADDR},
	.lvsys_int_rdb_sel_mask = MT6661_RG_LVSYS_INT1_RISING_DB_SEL_MASK <<
				  MT6661_RG_LVSYS_INT1_RISING_DB_SEL_SHIFT,
	.lvsys_int_vthl_reg = {MT6661_RG_LVSYS1_INT_VTHL_ADDR,
			       MT6661_RG_LVSYS2_INT_VTHL_ADDR},
	.lvsys_int_vthl_mask = {MT6661_RG_LVSYS1_INT_VTHL_MASK << MT6661_RG_LVSYS1_INT_VTHL_SHIFT,
				MT6661_RG_LVSYS2_INT_VTHL_MASK << MT6661_RG_LVSYS2_INT_VTHL_SHIFT},
	.lvsys_int_vthh_reg = {MT6661_RG_LVSYS1_INT_VTHH_ADDR,
			       MT6661_RG_LVSYS2_INT_VTHH_ADDR},
	.lvsys_int_vthh_mask = {MT6661_RG_LVSYS1_INT_VTHH_MASK << MT6661_RG_LVSYS1_INT_VTHH_SHIFT,
				MT6661_RG_LVSYS2_INT_VTHH_MASK << MT6661_RG_LVSYS2_INT_VTHH_SHIFT},
	.lvsys_int_o_sel_reg = MT6661_RG_LVSYS_INT_O_SEL_ADDR,
	.lvsys_int_o_sel_mask = MT6661_RG_LVSYS_INT_O_SEL_MASK,
	.lvsys_int_o_sel_shift = MT6661_RG_LVSYS_INT_O_SEL_SHIFT,
	.lvsys_int_notify = {SPMI_RCS, SPMI_RCS, SPMI_RCS, SPMI_RCS},
	.tma_key_reg = MT6661_TOP_TMA_KEY_L,
	.tma_key_unlock_val = MT6661_TMA_UNLOCK_VALUE,
	.vthl_range = {
		{
			.min = 2900,
			.min_sel = 0,
			.max_sel = 11,
			.step = 100,
		}, {
			.min = 2600,
			.min_sel = 0,
			.max_sel = 11,
			.step = 100,
		},
	},
	.vthh_range = {
		{
			.min = 3200,
			.min_sel = 0,
			.max_sel = 11,
			.step = 100,
		}, {
			.min = 2900,
			.min_sel = 0,
			.max_sel = 11,
			.step = 100,
		},
	},
};

struct pmic_lvsys_notify {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	const struct pmic_lvsys_info *info;
	unsigned int *thd_volts_l;
	unsigned int *thd_volts_h;
	unsigned int *cur_lv_ptr;
	unsigned int *cur_hv_ptr;
	int thd_volts_l_size;
	int thd_volts_h_size;
	int bat_type;
};

struct pmic_lvsys_notify *lvsys_notify;

static BLOCKING_NOTIFIER_HEAD(lvsys_notifier_list);

/**
 *	lvsys_register_notifier - register a lvsys notifier
 *	@nb: notifier block to callback on events
 *
 *	Return: 0 on success, negative error code on failure.
 */
int lvsys_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lvsys_notifier_list, nb);
}
EXPORT_SYMBOL(lvsys_register_notifier);

/**
 *	lvsys_unregister_notifier - unregister a lvsys notifier
 *	@nb: notifier block to callback on events
 *
 *	Return: 0 on success, negative error code on failure.
 */
int lvsys_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lvsys_notifier_list, nb);
}
EXPORT_SYMBOL(lvsys_unregister_notifier);

#if LVSYS_VIO18_SWITCH
struct vio18_ctrl_t {
	struct notifier_block nb;
	struct regmap *main_regmap;
	struct regmap *second_regmap;
	unsigned int main_switch;
	unsigned int second_switch;
};

static int vio18_switch_handler(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret;
	struct vio18_ctrl_t *vio18_ctrl = container_of(nb, struct vio18_ctrl_t, nb);

	if (event == LVSYS_F_2900) {
		ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 0);
		if (ret)
			pr_notice("Failed to set main vio18_switch, ret:%d\n", ret);

		ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 0);
		if (ret)
			pr_notice("Failed to set second vio18_switch, ret:%d\n", ret);
	} else if (event == LVSYS_R_3100) {
		ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 1);
		if (ret)
			pr_notice("Failed to set main vio18_switch, ret:%d\n", ret);

		ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 1);
		if (ret)
			pr_notice("Failed to set second vio18_switch, ret:%d\n", ret);
	}
	return 0;
}

static struct regmap *vio18_switch_get_regmap(const char *name)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_node_by_name(NULL, name);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np->child);
	if (!pdev)
		return NULL;

	return dev_get_regmap(pdev->dev.parent, NULL);
}

static int vio18_switch_init(struct device *dev, struct regmap *main_regmap)
{
	int ret = 0;
	unsigned int val_arr[2] = {0};
	struct vio18_ctrl_t *vio18_ctrl;

	vio18_ctrl = devm_kzalloc(dev, sizeof(*vio18_ctrl), GFP_KERNEL);
	if (!vio18_ctrl)
		return -ENOMEM;

	vio18_ctrl->main_regmap = main_regmap;
	vio18_ctrl->second_regmap = vio18_switch_get_regmap("second_pmic");
	if (!vio18_ctrl->second_regmap)
		return -EINVAL;

	ret = of_property_read_u32_array(dev->of_node, "vio18-switch-reg", val_arr, 2);
	if (ret)
		return -EINVAL;

	vio18_ctrl->main_switch = val_arr[0];
	vio18_ctrl->second_switch = val_arr[1];
	vio18_ctrl->nb.notifier_call = vio18_switch_handler;

	return lvsys_register_notifier(&vio18_ctrl->nb);
}
#endif /* LVSYS_VIO18_SWITCH */

void dump_lvsys_thd(void)
{
	int i = 0;

	pr_info("[%s] thd_volts_l_size: %d, thd_volts_h_size: %d\n", __func__,
		lvsys_notify->thd_volts_l_size, lvsys_notify->thd_volts_h_size);
	pr_info("[%s] thd_l: ", __func__);
	for (i = 0; i < lvsys_notify->thd_volts_l_size; i++)
		pr_info("%d ", lvsys_notify->thd_volts_l[i]);
	pr_info("[%s] thd_h: ", __func__);
	for (i = 0; i < lvsys_notify->thd_volts_h_size; i++)
		pr_info("%d ", lvsys_notify->thd_volts_h[i]);
}
EXPORT_SYMBOL(dump_lvsys_thd);

static bool is_multi_level_lvsys(void)
{
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	unsigned int hwcid = 0;

	regmap_read(lvsys_notify->regmap, info->hwcid, &hwcid);
	/* 6363 is single level and 6661 is multi level */
	if (hwcid == 0x63)
		return false;
	return true;
}

static int lvsys_unlock(bool unlock)
{
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	u16 buf = 0;

	if (info && info->tma_key_reg) {
		buf = unlock ? info->tma_key_unlock_val : 0;
		return regmap_bulk_write(lvsys_notify->regmap,
					 info->tma_key_reg, &buf, 2);
	}
	return -1;
}

static void enable_lvsys_int(bool en)
{
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	unsigned int i = 0, w_val;
	int ret;
#if LVSYS_DBG
	unsigned int val = 0;
#endif

	lvsys_unlock(true);
	for (i = 0; i < LVSYS_INT_EN_SIZE; i++) {
		if (info->lvsys_int_en_reg[i]) {
			w_val = en ? info->lvsys_int_en_mask[i] : 0;
			ret = regmap_update_bits(lvsys_notify->regmap,
						 info->lvsys_int_en_reg[i],
						 info->lvsys_int_en_mask[i], w_val);
#if LVSYS_DBG
			regmap_read(lvsys_notify->regmap, info->lvsys_int_en_reg[i], &val);
			dev_notice(lvsys_notify->dev, "lvsys_int_en_reg[0x%x]: 0x%x\n",
				   info->lvsys_int_en_reg[i], val);
#endif
			if (ret)
				dev_notice(lvsys_notify->dev, "Failed to enable LVSYS_INT, addr:0x%x, ret:%d\n",
					   info->lvsys_int_en_reg[i], ret);
		}
	}
	lvsys_unlock(false);
}

static void lvsys_int_notification_setting(void)
{
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	unsigned int i = 0;
	int ret;
#if LVSYS_DBG
	unsigned int val = 0;
#endif

	if (!info->lvsys_int_o_sel_reg)
		return;
	for (i = 0; i < LVSYS_INT_FALLING_SIZE + LVSYS_INT_RISING_SIZE; i++) {
		if (info->lvsys_int_notify[i] == GPIO) {
			ret = regmap_update_bits(lvsys_notify->regmap,
						 info->lvsys_int_o_sel_reg,
						 info->lvsys_int_o_sel_mask << info->lvsys_int_o_sel_shift,
						 i << info->lvsys_int_o_sel_shift);
#if LVSYS_DBG
			regmap_read(lvsys_notify->regmap, info->lvsys_int_o_sel_reg, &val);
			dev_notice(lvsys_notify->dev, "lvsys_int_o_sel_reg[0x%x]: 0x%x\n",
				   info->lvsys_int_o_sel_reg, val);
#endif
			if (ret)
				dev_notice(lvsys_notify->dev, "Failed to set RG_LVSYS_INT_O_SEL, addr:0x%x, ret:%d\n",
					   info->lvsys_int_o_sel_reg, ret);
		}
	}
}

static int lvsys_vth_get_selector_high(const struct linear_range *r,
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

static int lvsys_vth_get_selector_low(const struct linear_range *r,
				      unsigned int val, unsigned int *selector)
{
	if (r->min > val)
		return -EINVAL;

	if ((r->min + (r->max_sel - r->min_sel) * r->step) < val) {
		*selector = r->max_sel;
		return 0;
	}
	if (r->step == 0)
		*selector = r->min_sel;
	else
		*selector = (val - r->min) / r->step + r->min_sel;

	return 0;
}

static unsigned int get_cur_lv_idx(void)
{
	int thd_volt_size = lvsys_notify->thd_volts_l_size;
	unsigned int i = 0;

	for (i = 0; i < thd_volt_size; i++) {
		if (lvsys_notify->thd_volts_l[i] == *(lvsys_notify->cur_lv_ptr))
			return i;
	}
	return -1;
}

static unsigned int get_cur_hv_idx(void)
{
	int thd_volt_size = lvsys_notify->thd_volts_h_size;
	unsigned int i = 0;

	for (i = 0; i < thd_volt_size; i++) {
		if (lvsys_notify->thd_volts_h[i] == *(lvsys_notify->cur_hv_ptr))
			return i;
	}
	return -1;
}

static unsigned int *get_next_lv_ptr(void)
{
	unsigned int i = get_cur_hv_idx();

	if (lvsys_notify->thd_volts_l[i])
		return &lvsys_notify->thd_volts_l[i];
	return NULL;
}

static unsigned int *get_next_hv_ptr(void)
{
	unsigned int i = get_cur_lv_idx();

	if (lvsys_notify->thd_volts_h[i])
		return &lvsys_notify->thd_volts_h[i];
	return NULL;
}

static irqreturn_t lvsys_f_int_handler(int irq, void *data)
{
	unsigned int event;
	enum INT_NOTIFIY int_notify;

	if (!lvsys_notify)
		return IRQ_HANDLED;

	mutex_lock(&lvsys_notify->lock);
	if (!lvsys_notify->cur_lv_ptr) {
		mutex_unlock(&lvsys_notify->lock);
		return IRQ_HANDLED;
	}
	int_notify = lvsys_notify->info->lvsys_int_notify[(get_cur_lv_idx() * LVSYS_EDGE_NUM) + 1];
#if LVSYS_DBG
	dev_notice(lvsys_notify->dev, "lvsys_int_notify[%d]: %d\n",
		   (get_cur_lv_idx() * LVSYS_EDGE_NUM) + 1, int_notify);
#endif
	event = EVENT_LVSYS_F | *(lvsys_notify->cur_lv_ptr);
	if (int_notify == SPMI_RCS) {
		blocking_notifier_call_chain(&lvsys_notifier_list, event, NULL);
		dev_notice(lvsys_notify->dev, "event: falling %dmV(SPMI RCS)\n", *(lvsys_notify->cur_lv_ptr));
	} else if (int_notify == GPIO)
		dev_notice(lvsys_notify->dev, "event: falling %dmV(GPIO)\n", *(lvsys_notify->cur_lv_ptr));

	lvsys_notify->cur_hv_ptr = get_next_hv_ptr();
	if (lvsys_notify->cur_lv_ptr + 1 &&
	   (lvsys_notify->cur_lv_ptr + 1 <= lvsys_notify->thd_volts_l + lvsys_notify->thd_volts_l_size - 1)) {
		lvsys_notify->cur_lv_ptr++;
	}
#if LVSYS_DBG
	dev_notice(lvsys_notify->dev, "cur_hv: %d, cur_lv:%d\n",
		   *(lvsys_notify->cur_hv_ptr), *(lvsys_notify->cur_lv_ptr));
#endif
	mutex_unlock(&lvsys_notify->lock);

	return IRQ_HANDLED;
}

static irqreturn_t lvsys_r_int_handler(int irq, void *data)
{
	unsigned int event;
	enum INT_NOTIFIY int_notify;

	if (!lvsys_notify)
		return IRQ_HANDLED;

	mutex_lock(&lvsys_notify->lock);
	if (!lvsys_notify->cur_hv_ptr) {
		mutex_unlock(&lvsys_notify->lock);
		return IRQ_HANDLED;
	}
	int_notify = lvsys_notify->info->lvsys_int_notify[get_cur_hv_idx() * LVSYS_EDGE_NUM];
#if LVSYS_DBG
	dev_notice(lvsys_notify->dev, "lvsys_int_notify[%d]: %d\n",
		   get_cur_hv_idx() * LVSYS_EDGE_NUM, int_notify);
#endif
	event = EVENT_LVSYS_R | *(lvsys_notify->cur_hv_ptr);
	if (int_notify == SPMI_RCS) {
		blocking_notifier_call_chain(&lvsys_notifier_list, event, NULL);
		dev_notice(lvsys_notify->dev, "event: rising %dmV(SPMI RCS)\n", *(lvsys_notify->cur_hv_ptr));
	} else if (int_notify == GPIO)
		dev_notice(lvsys_notify->dev, "event: rising %dmV(GPIO)\n", *(lvsys_notify->cur_hv_ptr));

	lvsys_notify->cur_lv_ptr = get_next_lv_ptr();
	if (lvsys_notify->cur_hv_ptr - 1 &&
	   (lvsys_notify->cur_hv_ptr - 1 >= lvsys_notify->thd_volts_h)) {
		lvsys_notify->cur_hv_ptr--;
	}
#if LVSYS_DBG
	dev_notice(lvsys_notify->dev, "cur_hv: %d, cur_lv:%d\n",
		   *(lvsys_notify->cur_hv_ptr), *(lvsys_notify->cur_lv_ptr));
#endif
	mutex_unlock(&lvsys_notify->lock);

	return IRQ_HANDLED;
}

/* thd_vold_arr order: LVSYS1 rising/falling, LVSYS2 rising/falling */
int lvsys_modify_thd(unsigned int *thd_volt_arr, unsigned int thd_volt_size)
{
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	unsigned int arr_idx, i, vth_sel = 0;
	int ret = 0;
#if LVSYS_DBG
	u32 val = 0;
#endif

#if LVSYS_DBG
	for (i = 0; i < thd_volt_size; i++)
		pr_info("thd_volt_arr[%d]: %d\n", i, thd_volt_arr[i]);
	dump_lvsys_thd();
#endif
	if (!lvsys_notify->regmap) {
		dev_notice(lvsys_notify->dev, "failed to get regmap\n");
		return -EPROBE_DEFER;
	}

	for (arr_idx = 0; arr_idx < thd_volt_size; arr_idx++) {
		/* set modified threshold to 0 to skip */
		if (thd_volt_arr[arr_idx] == 0)
			continue;
		/* index of LVSYS comparator */
		i = arr_idx / LVSYS_EDGE_NUM;
		/* update rising threshold */
		if (arr_idx % LVSYS_EDGE_NUM == 0) {
			if (info->lvsys_int_vthh_reg[i] && info->lvsys_int_vthh_mask[i]) {
				ret = lvsys_vth_get_selector_low(&(info->vthh_range[i]),
								 thd_volt_arr[arr_idx], &vth_sel);
				if (ret < 0)
					dev_notice(lvsys_notify->dev, "Invalid parameter, ret:%d\n", ret);
				vth_sel <<= __ffs(info->lvsys_int_vthh_mask[i]);
#if LVSYS_DBG
				dev_info(lvsys_notify->dev, "set LVSYS%d_INT_VTHH to 0x%x\n",
					 i + 1, vth_sel);
#endif
				ret = regmap_update_bits(lvsys_notify->regmap,
							 info->lvsys_int_vthh_reg[i],
							 info->lvsys_int_vthh_mask[i], vth_sel);
				if (ret)
					dev_notice(lvsys_notify->dev, "Failed to set LVSYS%d_INT_VTHH to %d, ret:%d\n",
						   i + 1, vth_sel, ret);
#if LVSYS_DBG
				regmap_read(lvsys_notify->regmap, info->lvsys_int_vthh_reg[i], &val);
				dev_info(lvsys_notify->dev, "lvsys_int_vthh_reg[0x%x]: 0x%x\n",
					 info->lvsys_int_vthh_reg[i], val);
#endif
				lvsys_notify->thd_volts_h[i] = thd_volt_arr[arr_idx];
			} else
				dev_notice(lvsys_notify->dev, "Don't have lvsys_int_vthh_reg[%d]\n", i);
		/* update falling threshold */
		} else {
			if (info->lvsys_int_vthl_reg[i] && info->lvsys_int_vthl_mask[i]) {
				ret = lvsys_vth_get_selector_high(&(info->vthl_range[i]),
								  thd_volt_arr[arr_idx], &vth_sel);
				if (ret < 0)
					dev_notice(lvsys_notify->dev, "Invalid parameter, ret:%d\n", ret);
				vth_sel <<= __ffs(info->lvsys_int_vthl_mask[i]);
#if LVSYS_DBG
				dev_info(lvsys_notify->dev, "set LVSYS%d_INT_VTHL to 0x%x\n",
					 i + 1, vth_sel);
#endif
				ret = regmap_update_bits(lvsys_notify->regmap,
							 info->lvsys_int_vthl_reg[i],
							 info->lvsys_int_vthl_mask[i], vth_sel);
#if LVSYS_DBG
				regmap_read(lvsys_notify->regmap, info->lvsys_int_vthl_reg[i], &val);
				dev_info(lvsys_notify->dev, "lvsys_int_vthl_reg[0x%x]: 0x%x\n",
					info->lvsys_int_vthl_reg[i], val);
#endif
				if (ret)
					dev_notice(lvsys_notify->dev, "Failed to set LVSYS%d_INT_VTHL to %d, ret:%d\n",
						   i + 1, vth_sel, ret);
				lvsys_notify->thd_volts_l[i] = thd_volt_arr[arr_idx];
			} else
				dev_notice(lvsys_notify->dev, "Don't have lvsys_int_vthl_reg[%d]\n", i);
		}
	}

#if LVSYS_DBG
	for (i = 0; i < thd_volt_size; i++)
		pr_info("thd_volt_arr[%d]: %d\n", i, thd_volt_arr[i]);
	dump_lvsys_thd();
#endif
	return ret;
}
EXPORT_SYMBOL(lvsys_modify_thd);

int lvsys_modify_thd_locked(unsigned int *thd_volt_arr, unsigned int thd_volt_size)
{
	int ret;

	mutex_lock(&lvsys_notify->lock);
	ret = lvsys_modify_thd(thd_volt_arr, thd_volt_size);
	mutex_unlock(&lvsys_notify->lock);

	return ret;
}
EXPORT_SYMBOL(lvsys_modify_thd_locked);

static int pmic_lvsys_parse_dt(struct device_node *lvsys_np)
{
	struct device_node *np, *lowbatpt_np;
	const struct pmic_lvsys_info *info = lvsys_notify->info;
	unsigned int *thd_volt_arr;
	unsigned int deb_sel = 0;
	int ret, i = 0, j = 0, lvsys_thd_enable = 0, thd_volt_size = 0;
	char *lvsys_node_thd_l = "thd-volts-l";
	char *lvsys_node_thd_h = "thd-volts-h";
	char *lowbat_node_thd_l = "lvsys-thd-volt-l";
	char *lowbat_node_thd_h = "lvsys-thd-volt-h";
	char *thd_l = NULL, *thd_h = NULL;
#if LVSYS_DBG
	u32 val = 0;
#endif

	lowbatpt_np = of_find_node_by_name(NULL, "low-battery-throttling");
	if (!lowbatpt_np) {
		dev_notice(lvsys_notify->dev, "get low-battery-throttling node fail\n");
		np = lvsys_np;
		thd_l = lvsys_node_thd_l;
		thd_h = lvsys_node_thd_h;
	} else {
		ret = of_property_read_u32(lowbatpt_np, "lvsys-thd-enable", &lvsys_thd_enable);
		if (ret) {
			dev_notice(lvsys_notify->dev, "[%s] failed to get lvsys-thd-enable ret:%d\n",
				   __func__, ret);
			lvsys_thd_enable = 0;
		}
		/* use lvsys thd in low-battery-throttling first */
		if (lvsys_thd_enable) {
			np = lowbatpt_np;
			thd_l = lowbat_node_thd_l;
			thd_h = lowbat_node_thd_h;
		}
	}

	ret = of_property_read_u32(np, "lvsys-thd-volt-l-size", &lvsys_notify->thd_volts_l_size);
	if (ret)
		lvsys_notify->thd_volts_l_size =
			of_property_count_elems_of_size(np, thd_l, sizeof(u32));

	if (lvsys_notify->thd_volts_l_size <= 0) {
		dev_notice(lvsys_notify->dev, "[%s] ERROR: thd_volts_l_size < 0\n", __func__);
		return -EINVAL;
	}
	lvsys_notify->thd_volts_l = devm_kmalloc_array(lvsys_notify->dev,
						       lvsys_notify->thd_volts_l_size,
						       sizeof(u32), GFP_KERNEL);
	if (!lvsys_notify->thd_volts_l)
		return -ENOMEM;
	ret = of_property_read_u32_array(np, thd_l, lvsys_notify->thd_volts_l,
					 lvsys_notify->thd_volts_l_size);
	if (ret)
		dev_notice(lvsys_notify->dev, "[%s] failed to read lvsys-thd-volt-l ret:%d\n",
			   __func__, ret);

	ret = of_property_read_u32(np, "lvsys-thd-volt-h-size", &lvsys_notify->thd_volts_h_size);
	if (ret)
		lvsys_notify->thd_volts_h_size =
			of_property_count_elems_of_size(np, thd_h, sizeof(u32));
	if (lvsys_notify->thd_volts_h_size <= 0) {
		dev_notice(lvsys_notify->dev, "[%s] ERROR: thd_volts_h_size < 0\n", __func__);
		return -EINVAL;
	}
	lvsys_notify->thd_volts_h = devm_kmalloc_array(lvsys_notify->dev,
						       lvsys_notify->thd_volts_h_size,
						       sizeof(u32), GFP_KERNEL);
	if (!lvsys_notify->thd_volts_h)
		return -ENOMEM;
	ret = of_property_read_u32_array(np, thd_h, lvsys_notify->thd_volts_h,
					 lvsys_notify->thd_volts_h_size);
	if (ret)
		dev_notice(lvsys_notify->dev, "[%s] failed to read lvsys-thd-volt-h ret:%d\n",
			   __func__, ret);
#if LVSYS_DBG
	dump_lvsys_thd();
#endif
	lvsys_notify->cur_lv_ptr = lvsys_notify->thd_volts_l;
	lvsys_notify->cur_hv_ptr = lvsys_notify->thd_volts_h;

	thd_volt_size = lvsys_notify->thd_volts_l_size + lvsys_notify->thd_volts_h_size;
	thd_volt_arr = devm_kmalloc_array(lvsys_notify->dev, thd_volt_size,
					  sizeof(u32), GFP_KERNEL);
	if (!thd_volt_arr)
		return -ENOMEM;

	for (i = 0, j = 0; i < thd_volt_size; i += LVSYS_EDGE_NUM, j++) {
		thd_volt_arr[i] = lvsys_notify->thd_volts_h[j];
		thd_volt_arr[i+1] = lvsys_notify->thd_volts_l[j];
	}
	lvsys_modify_thd(thd_volt_arr, (unsigned int)thd_volt_size);

	ret = of_property_read_u32(lvsys_np, "lv-deb-sel", &deb_sel);
	if (ret)
		deb_sel = 0;
	else
		deb_sel <<= __ffs(info->lvsys_int_fdb_sel_mask);
	for (i = 0; i < LVSYS_INT_FALLING_SIZE; i++) {
		if (info->lvsys_int_fdb_sel_reg[i]) {
			ret = regmap_update_bits(lvsys_notify->regmap,
						info->lvsys_int_fdb_sel_reg[i],
						info->lvsys_int_fdb_sel_mask, deb_sel);
#if LVSYS_DBG
			regmap_read(lvsys_notify->regmap, info->lvsys_int_fdb_sel_reg[i], &val);
			pr_info("%s lvsys_int_fdb_sel_reg[%d]: 0x%x\n", __func__, i, val);
#endif
			if (ret)
				dev_notice(lvsys_notify->dev, "Failed to set LVSYS_INT_FDB_SEL, ret:%d\n", ret);
		}
	}

	ret = of_property_read_u32(lvsys_np, "hv-deb-sel", &deb_sel);
	if (ret)
		deb_sel = 0;
	else
		deb_sel <<= __ffs(info->lvsys_int_rdb_sel_mask);
	for (i = 0; i < LVSYS_INT_RISING_SIZE; i++) {
		if (info->lvsys_int_rdb_sel_reg[i]) {
			ret = regmap_update_bits(lvsys_notify->regmap,
						info->lvsys_int_rdb_sel_reg[i],
						info->lvsys_int_rdb_sel_mask, deb_sel);
#if LVSYS_DBG
			regmap_read(lvsys_notify->regmap, info->lvsys_int_rdb_sel_reg[i], &val);
			pr_info("%s lvsys_int_rdb_sel_reg[%d]: 0x%x\n", __func__, i, val);
#endif
			if (ret)
				dev_notice(lvsys_notify->dev, "Failed to set LVSYS_INT_RDB_SEL, ret:%d\n", ret);
		}
	}

	return ret;
}

static int pmic_lvsys_notify_probe(struct platform_device *pdev)
{
	const struct pmic_lvsys_info *info;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *gauge_np;
	unsigned int lvsys_int_size = 0, i = 0;
	int ret, irq;
	static const char **lvsys_int;
	static const char *single_lvsys_int[] = {"LVSYS_R", "LVSYS_F"};
	static const char *multi_lvsys_int[] = {"LVSYS1_R", "LVSYS1_F", "LVSYS2_R", "LVSYS2_F"};

	lvsys_notify = devm_kzalloc(&pdev->dev, sizeof(*lvsys_notify), GFP_KERNEL);
	if (!lvsys_notify)
		return -ENOMEM;

	info = of_device_get_match_data(&pdev->dev);
	if (!info) {
		dev_notice(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}
	lvsys_notify->dev = &pdev->dev;
	lvsys_notify->info = info;
	lvsys_notify->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!lvsys_notify->regmap) {
		dev_notice(&pdev->dev, "failed to get regmap\n");
		return -ENODEV;
	}

	gauge_np = of_find_node_by_name(NULL, "mtk-gauge");
	if (!gauge_np)
		dev_notice(&pdev->dev, "get mtk-gauge node fail\n");
	else {
		ret = of_property_read_u32(gauge_np, "bat_type", &lvsys_notify->bat_type);
		if (ret)
			dev_notice(&pdev->dev, "get bat_type fail\n");
	}
	ret = pmic_lvsys_parse_dt(np);
	if (ret)
		dev_notice(&pdev->dev, "parse lvsys DT failed, ret:%d\n", ret);
	mutex_init(&lvsys_notify->lock);

	if (is_multi_level_lvsys()) {
		lvsys_int = multi_lvsys_int;
		lvsys_int_size = sizeof(multi_lvsys_int) / sizeof(multi_lvsys_int[0]);
	} else {
		lvsys_int = single_lvsys_int;
		lvsys_int_size = sizeof(single_lvsys_int) / sizeof(single_lvsys_int[0]);
	}
	for (i = 0; i < lvsys_int_size; i++) {
		irq = platform_get_irq_byname_optional(pdev, lvsys_int[i]);
		if (irq < 0) {
			dev_notice(&pdev->dev, "failed to get %s irq, ret:%d\n", lvsys_int[i], irq);
			return irq;
		}
		if (strstr(lvsys_int[i], "_F")) {
#if LVSYS_DBG
			pr_info("%s falling lvsys_int[%d]: %s matched\n",
				__func__, i, lvsys_int[i]);
#endif
			ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
							lvsys_f_int_handler, IRQF_ONESHOT,
							lvsys_int[i], NULL);
		} else if (strstr(lvsys_int[i], "_R")) {
#if LVSYS_DBG
			pr_info("%s rising lvsys_int[%d]: %s matched\n",
				__func__, i, lvsys_int[i]);
#endif
			ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
							lvsys_r_int_handler, IRQF_ONESHOT,
							lvsys_int[i], NULL);
		} else
			dev_notice(&pdev->dev, "failed to match lvsys_int[%d]: %s\n", i, lvsys_int[i]);
		if (ret < 0) {
			dev_notice(&pdev->dev, "failed to request %s irq, ret:%d\n", lvsys_int[i], ret);
			return ret;
		}
	}

	/* RG_LVSYS_INT_EN = 0x1 */
	enable_lvsys_int(true);
	lvsys_int_notification_setting();

#if LVSYS_VIO18_SWITCH
	ret = vio18_switch_init(&pdev->dev, lvsys_notify->regmap);
	if (ret)
		dev_notice(&pdev->dev, "vio18_switch_init failed, ret:%d\n", ret);
#endif
	return 0;
}

static const struct of_device_id pmic_lvsys_notify_of_match[] = {
	{
		.compatible = "mediatek,mt6363-lvsys-notify",
		.data = &mt6363_lvsys_info,
	}, {
		.compatible = "mediatek,mt6661-lvsys-notify",
		.data = &mt6661_lvsys_info,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, pmic_lvsys_notify_of_match);

static struct platform_driver pmic_lvsys_notify_driver = {
	.driver = {
		.name = "pmic_lvsys_notify",
		.of_match_table = pmic_lvsys_notify_of_match,
	},
	.probe	= pmic_lvsys_notify_probe,
};
module_platform_driver(pmic_lvsys_notify_driver);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK pmic lvsys notify driver");
MODULE_LICENSE("GPL");
