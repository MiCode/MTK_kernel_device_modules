// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"
#include "pmic_lvsys_notify.h"

#define CREATE_TRACE_POINTS
#include "mtk_low_battery_throttling_trace.h"
#include "../mbraink/mbraink_ioctl_struct_def.h"

#define LBCB_MAX_NUM 16
#define TEMP_MAX_STAGE_NUM 6
#define THD_VOLTS_LENGTH 30
#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100
#define POWER_INT3_VOLT 2700
#define LVSYS_THD_VOLT_H 3100
#define LVSYS_THD_VOLT_L 2900
#define MAX_INT 0x7FFFFFFF
#define MIN_LBAT_VOLT 2000
#define LOW_BATTERY_INIT_NUM (LOW_BATTERY_LEVEL_NUM-2)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define VOLT_L_STR "thd-volts-l"
#define VOLT_H_STR "thd-volts-h"

struct lbat_intr_tbl {
	unsigned int volt_thd;
	unsigned int lt_en;
	unsigned int lt_lv;
	unsigned int ht_en;
	unsigned int ht_lv;
};

struct lbat_thd_tbl {
	unsigned int *thd_volts;
	unsigned int *thd_level;
	int thd_volts_size;
	int thd_level_size;
	struct lbat_intr_tbl *lbat_intr_info;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct low_bat_thl_priv {
	struct tag_bootmode *tag;
	bool notify_flag;
	struct wait_queue_head notify_waiter;
	struct timer_list notify_timer;
	struct task_struct *notify_thread;
	struct device *dev;
	int low_bat_thl_level;
	int lbat_thl_intr_level;
	int lvsys_thl_intr_level;
	int low_bat_thl_stop;
	int low_bat_thd_modify;
	unsigned int lvsys_thd_volt_l;
	unsigned int lvsys_thd_volt_h;
	unsigned int ppb_mode;
	struct work_struct temp_work;
	struct power_supply *psy;
	int *temp_thd;
	int temp_max_stage;
	int temp_cur_stage;
	int temp_reg_stage;
	unsigned int aging_factor_enable;
	unsigned int aging_factor;
	unsigned int *aging_factor_volts;
	unsigned int max_thd_lv[INTR_MAX_NUM];
	unsigned int pmic_idx[INTR_MAX_NUM];
	unsigned int lbat_intr_num;
	int lbat_level[INTR_MAX_NUM];
	struct lbat_mbrain lbat_mbrain_info;
	struct lbat_user *lbat_pt[INTR_MAX_NUM];
	struct lbat_thd_tbl lbat_thd_info[INTR_MAX_NUM][TEMP_MAX_STAGE_NUM];
};

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG, void *data);
	void *data;
};

static struct notifier_block lbat_nb;
static struct notifier_block bp_nb;
static struct low_bat_thl_priv *low_bat_thl_data;
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0}, {0} };
static low_battery_mbrain_callback lb_mbrain_cb;
static DEFINE_MUTEX(exe_thr_lock);

static int rearrange_volt(struct lbat_intr_tbl *intr_info, unsigned int *volt_l,
	unsigned int *volt_h, unsigned int *thd_level, unsigned int num)
{
	unsigned int idx_l = 0, idx_h = 0, idx_t = 0, lv_idx = 0, i;
	unsigned int volt_l_next, volt_h_next, thd_level_next;
	unsigned int thd_level_cnt = 0;

	for (i = 0; i < num - 1; i++) {
		if (volt_l[i] < volt_l[i+1] || volt_h[i] < volt_h[i+1]) {
			pr_notice("[%s] i=%d volt_l(%d, %d) volt_h(%d, %d) error\n",
				__func__, i, volt_l[i], volt_l[i+1], volt_h[i], volt_h[i+1]);
			return -EINVAL;
		}
	}
	for (i = 0; i < num * 2; i++) {
		volt_l_next = (idx_l < num) ? volt_l[idx_l] : 0;
		volt_h_next = (idx_h < num) ? volt_h[idx_h] : 0;
		thd_level_next = (lv_idx < LOW_BATTERY_INIT_NUM) ? thd_level[lv_idx] : 0;
		if (volt_l_next > volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = thd_level_next;
			thd_level_cnt++;
			if (thd_level_cnt == 2)
				lv_idx++;
			idx_l++;
			idx_t++;
		} else if (volt_l_next == volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = thd_level_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = thd_level_next;
			lv_idx++;
			idx_l++;
			idx_h++;
			idx_t++;
		} else if (volt_h_next > 0) {
			intr_info[idx_t].volt_thd = volt_h_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = thd_level_next;
			lv_idx++;
			thd_level_cnt = 0;
			idx_h++;
			idx_t++;
		} else
			break;
	}
	for (i = 0; i < idx_t; i++) {
		pr_info("[%s] intr_info[%d] = (%d, trig l[%d %d] h[%d %d])\n",
			__func__, i, intr_info[i].volt_thd, intr_info[i].lt_en,
			intr_info[i].lt_lv, intr_info[i].ht_en, intr_info[i].ht_lv);
	}
	return idx_t;
}

static void __used dump_thd_volts(struct device *dev, unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	dev_notice(dev, "%s Done\n", str);
}

static void __used dump_thd_volts_ext(unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	pr_info("%s Done\n", str);
}

static void __used dump_aging_factor(unsigned int *aging_factor_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	r = snprintf(str + r, len - r, "aging factor volts: ");

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%umV ", aging_factor_volts[i]);
		if (r >= len)
			return;
	}
	pr_info("%s Done\n", str);
}

static int __used check_aging_factor_order(unsigned int aging_factor)
{
	int i, j, k, aging_factor_diff, aging_factor_volt;
	unsigned int *aging_factor_volts;
	struct lbat_thd_tbl lbat_thd_info;
	struct lbat_thd_tbl lbat_thd_info_cpy;

	lbat_thd_info_cpy.thd_volts = kcalloc(LOW_BATTERY_INIT_NUM, sizeof(unsigned int),
			GFP_KERNEL);
	lbat_thd_info_cpy.thd_volts_size = LOW_BATTERY_INIT_NUM;

	aging_factor_diff = aging_factor - low_bat_thl_data->aging_factor;
	aging_factor_volts = low_bat_thl_data->aging_factor_volts;

	for (i = 0; i < low_bat_thl_data->lbat_intr_num; i++) {
		for (j = 0; j <= low_bat_thl_data->temp_max_stage; j++) {
			lbat_thd_info = low_bat_thl_data->lbat_thd_info[i][j];
			for (k = 0; k < lbat_thd_info_cpy.thd_volts_size; k++) {
				lbat_thd_info_cpy.thd_volts[k] = lbat_thd_info.thd_volts[k];
				aging_factor_volt = aging_factor_diff * aging_factor_volts[k];
				lbat_thd_info_cpy.thd_volts[k] += aging_factor_volt;
				if (k != 0) {
					if(lbat_thd_info_cpy.thd_volts[k - 1]
						< lbat_thd_info_cpy.thd_volts[k]) {
						kfree(lbat_thd_info_cpy.thd_volts);
						return -1;
					}
				}
			}
		}
	}

	kfree(lbat_thd_info_cpy.thd_volts);
	return 0;
}

static void __used apply_aging_factor(unsigned int aging_factor, enum LOW_BATTERY_USER_TAG user)
{
	int i, j, ret, aging_factor_diff, aging_factor_volt;
	unsigned int *aging_factor_volts = low_bat_thl_data->aging_factor_volts;
	struct lbat_thd_tbl *thd_info;

	if (!low_bat_thl_data->aging_factor_enable && user != UT)
		return;

	if (aging_factor == low_bat_thl_data->aging_factor) {
		pr_info("same aging factor\n");
		return;
	}

	ret = check_aging_factor_order(aging_factor);
	if (ret != 0) {
		pr_info("check_anging_factor fail\n");
		return;
	}

	aging_factor_diff = aging_factor - low_bat_thl_data->aging_factor;
	low_bat_thl_data->aging_factor = aging_factor;

	mutex_lock(&exe_thr_lock);
	for (i = 0; i <= low_bat_thl_data->temp_max_stage; i++) {
		for (j = 0; j < low_bat_thl_data->lbat_thd_info[INTR_1][i].thd_volts_size; j++) {
			aging_factor_volt = aging_factor_diff * aging_factor_volts[j];
			low_bat_thl_data->lbat_thd_info[INTR_1][i].thd_volts[j] += aging_factor_volt;
			low_bat_thl_data->lbat_thd_info[INTR_1][i].lbat_intr_info[j].volt_thd
				+= aging_factor_volt;
		}
#ifdef LBAT2_ENABLE
		if (low_bat_thl_data->lbat_intr_num == 2) {
			for (j = 0; j < low_bat_thl_data->lbat_thd_info[INTR_2][i].thd_volts_size; j++) {
				aging_factor_volt = aging_factor_diff * aging_factor_volts[j];
				low_bat_thl_data->lbat_thd_info[INTR_2][i].thd_volts[j] += aging_factor_volt;
				low_bat_thl_data->lbat_thd_info[INTR_2][i].lbat_intr_info[j].volt_thd
					+= aging_factor_volt;
			}
		}
#endif
	}
	mutex_unlock(&exe_thr_lock);

	thd_info = &low_bat_thl_data->lbat_thd_info[INTR_1][low_bat_thl_data->temp_cur_stage];
	lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[INTR_1], thd_info->thd_volts,
			thd_info->thd_volts_size);
	dump_thd_volts_ext(thd_info->thd_volts, thd_info->thd_volts_size);
#ifdef LBAT2_ENABLE
	thd_info = &low_bat_thl_data->lbat_thd_info[low_bat_thl_data->temp_cur_stage];
	dual_lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[INTR_2], thd_info->thd_volts,
			thd_info->thd_volts_size);
	dump_thd_volts_ext(thd_info->thd_volts, thd_info->thd_volts_size);
#endif
}

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val, void *data)
{
	if (prio_val >= LBCB_MAX_NUM) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return -EINVAL;
	}

	if (!lb_cb)
		return -EINVAL;

	if (lbcb_tb[prio_val].lbcb != 0)
		pr_info("[%s] Notice: LBCB has been registered\n", __func__);

	lbcb_tb[prio_val].lbcb = lb_cb;
	lbcb_tb[prio_val].data = data;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (!low_bat_thl_data) {
		pr_info("[%s] Failed to create low_bat_thl_data\n", __func__);
		return 3;
	}

	if (low_bat_thl_data->low_bat_thl_level && lbcb_tb[prio_val].lbcb) {
		lbcb_tb[prio_val].lbcb(low_bat_thl_data->low_bat_thl_level, lbcb_tb[prio_val].data);
		pr_info("[%s] notify lv=%d\n", __func__, low_bat_thl_data->low_bat_thl_level);
	}
	return 3;
}
EXPORT_SYMBOL(register_low_battery_notify);

int register_low_battery_mbrain_cb(low_battery_mbrain_callback cb)
{
	if (!cb)
		return -EINVAL;

	lb_mbrain_cb = cb;

	return 0;
}
EXPORT_SYMBOL(register_low_battery_mbrain_cb);

void exec_throttle(unsigned int level, enum LOW_BATTERY_USER_TAG user, unsigned int thd_volt)
{
	int i;

	if (!low_bat_thl_data) {
		pr_info("[%s] Failed to create low_bat_thl_data\n", __func__);
		return;
	}

	if (low_bat_thl_data->low_bat_thl_level == level) {
		pr_info("[%s] same throttle level\n", __func__);
		return;
	}

	low_bat_thl_data->lbat_mbrain_info.user = user;
	low_bat_thl_data->lbat_mbrain_info.level = level;
	low_bat_thl_data->lbat_mbrain_info.thd_volt = thd_volt;

	low_bat_thl_data->low_bat_thl_level = level;
	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb)
			lbcb_tb[i].lbcb(low_bat_thl_data->low_bat_thl_level, lbcb_tb[i].data);
	}
	trace_low_battery_throttling_level(low_bat_thl_data->low_bat_thl_level);

	if (lb_mbrain_cb)
		lb_mbrain_cb(low_bat_thl_data->lbat_mbrain_info);

	pr_info("[%s] low_battery_level = %d\n", __func__, level);
}

static int __used decide_and_throttle(enum LOW_BATTERY_USER_TAG user,
					unsigned int input,
					unsigned int thd_volt)
{
	struct lbat_thd_tbl *thd_info[INTR_MAX_NUM];
	unsigned int low_thd_volts[LOW_BATTERY_INIT_NUM] = {MIN_LBAT_VOLT+40,
		MIN_LBAT_VOLT+30, MIN_LBAT_VOLT+20, MIN_LBAT_VOLT+10};
	int temp_cur_stage = 0;

	pr_info("%s: user=%d, input=%d\n", __func__, user, input);
	if (!low_bat_thl_data) {
		pr_info("[%s] Failed to create low_bat_thl_data\n", __func__);
		return -ENODATA;
	}

	mutex_lock(&exe_thr_lock);
	if (user == LBAT_INTR_1 || user == LBAT_INTR_2) {
		if (user == LBAT_INTR_1)
			low_bat_thl_data->lbat_level[INTR_1] = input;
		else if (user == LBAT_INTR_2)
			low_bat_thl_data->lbat_level[INTR_2] = input;

		if (low_bat_thl_data->lbat_level[INTR_2] > low_bat_thl_data->lbat_level[INTR_1])
			low_bat_thl_data->lbat_thl_intr_level = low_bat_thl_data->lbat_level[INTR_2];
		else
			low_bat_thl_data->lbat_thl_intr_level = low_bat_thl_data->lbat_level[INTR_1];

		if (low_bat_thl_data->low_bat_thl_stop > 0 || low_bat_thl_data->ppb_mode == 1) {
			pr_info("[%s] throttle not apply, low_bat_thl_stop=%d, ppb_mode=%d\n",
			__func__, low_bat_thl_data->low_bat_thl_stop,
			low_bat_thl_data->ppb_mode);
		} else {
			input = MAX(low_bat_thl_data->lbat_thl_intr_level,
						low_bat_thl_data->lvsys_thl_intr_level);
			exec_throttle(input, user, thd_volt);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == LVSYS_INTR) {
		low_bat_thl_data->lvsys_thl_intr_level = input;
		if (low_bat_thl_data->low_bat_thl_stop > 0 || low_bat_thl_data->ppb_mode == 1) {
			pr_info("[%s] low_bat_thl_stop=%d, ppb_mode=%d\n",
			__func__, low_bat_thl_data->low_bat_thl_stop,
			low_bat_thl_data->ppb_mode);
		} else {
			input = MAX(low_bat_thl_data->lbat_thl_intr_level,
						low_bat_thl_data->lvsys_thl_intr_level);
			exec_throttle(input, user, thd_volt);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == PPB) {
		low_bat_thl_data->ppb_mode = input;
		if (low_bat_thl_data->low_bat_thl_stop > 0) {
			pr_info("[%s] ppb not apply, low_bat_thl_stop=%d\n", __func__,
				low_bat_thl_data->low_bat_thl_stop);
			mutex_unlock(&exe_thr_lock);
		} else if (low_bat_thl_data->ppb_mode == 1) {
			low_bat_thl_data->lbat_thl_intr_level = 0;
			exec_throttle(LOW_BATTERY_LEVEL_0, user, thd_volt);
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[INTR_1],
				&low_thd_volts[0], LOW_BATTERY_INIT_NUM);
#ifdef LBAT2_ENABLE
			if (low_bat_thl_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked(
					low_bat_thl_data->lbat_pt[INTR_2], &low_thd_volts[0],
					LOW_BATTERY_INIT_NUM);
			}
#endif
			dump_thd_volts_ext(&low_thd_volts[0], LOW_BATTERY_INIT_NUM);
		} else {
			input = MAX(low_bat_thl_data->lbat_thl_intr_level,
						low_bat_thl_data->lvsys_thl_intr_level);
			exec_throttle(input, user, thd_volt);

			temp_cur_stage = low_bat_thl_data->temp_cur_stage;
			thd_info[INTR_1] = &low_bat_thl_data->lbat_thd_info[INTR_1][temp_cur_stage];
			thd_info[INTR_2] = &low_bat_thl_data->lbat_thd_info[INTR_2][temp_cur_stage];
			low_bat_thl_data->temp_reg_stage = temp_cur_stage;
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[INTR_1],
				thd_info[INTR_1]->thd_volts, thd_info[INTR_1]->thd_volts_size);
			dump_thd_volts_ext(thd_info[INTR_1]->thd_volts, thd_info[INTR_1]->thd_volts_size);
#ifdef LBAT2_ENABLE
			if (low_bat_thl_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked(
					low_bat_thl_data->lbat_pt[INTR_2], thd_info[INTR_2]->thd_volts,
					thd_info[INTR_2]->thd_volts_size);
				dump_thd_volts_ext(thd_info[INTR_2]->thd_volts, thd_info[INTR_2]->thd_volts_size);
			}
#endif
		}
	} else if (user == UT) {
		low_bat_thl_data->low_bat_thl_stop = 1;
		exec_throttle(input, user, thd_volt);
		mutex_unlock(&exe_thr_lock);
	} else {
		mutex_unlock(&exe_thr_lock);
	}
	return 0;
}

static unsigned int thd_to_level(unsigned int thd, unsigned int temp_stage,
	enum LOW_BATTERY_USER_TAG intr_type)
{
	unsigned int i, level = 0;
	struct lbat_intr_tbl *info;
	struct lbat_thd_tbl *thd_info;
	unsigned int *pmic_idx;

	if (!low_bat_thl_data)
		return 0;

	if (intr_type == LBAT_INTR_1) {
		thd_info = &low_bat_thl_data->lbat_thd_info[INTR_1][temp_stage];
		pmic_idx = &low_bat_thl_data->pmic_idx[INTR_1];
	} else if (intr_type == LBAT_INTR_2) {
		thd_info = &low_bat_thl_data->lbat_thd_info[INTR_2][temp_stage];
		pmic_idx = &low_bat_thl_data->pmic_idx[INTR_2];
	} else {
		pr_notice("[%s] wrong intr_type=%d\n", __func__, intr_type);
		return -1;
	}

	for (i = 0; i < thd_info->thd_volts_size; i++) {
		info = &thd_info->lbat_intr_info[i];
		if (thd == thd_info->thd_volts[i]) {
			*pmic_idx = i;
			if (info->ht_en == 1)
				level = info->ht_lv;
			else if (info->lt_en == 1)
				level = info->lt_lv;
			break;
		}
	}

	if (i == thd_info->thd_volts_size) {
		pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
		return -1;
	}

	return level;
}

void exec_low_battery_callback(unsigned int thd)
{
	unsigned int level = 0;

	if (!low_bat_thl_data) {
		pr_info("[%s] low_bat_thl_data not allocate\n", __func__);
		return;
	}

	level = thd_to_level(thd, low_bat_thl_data->temp_reg_stage, LBAT_INTR_1);
	if (level == -1)
		return;

	decide_and_throttle(LBAT_INTR_1, level, thd);
}

void exec_dual_low_battery_callback(unsigned int thd)
{
	unsigned int level = 0;

	if (!low_bat_thl_data) {
		pr_info("[%s] low_bat_thl_data not allocate\n", __func__);
		return;
	}

	level = thd_to_level(thd, low_bat_thl_data->temp_reg_stage, LBAT_INTR_2);
	if (level == -1)
		return;

	decide_and_throttle(LBAT_INTR_2, level, thd);
}

int lbat_set_ppb_mode(unsigned int mode)
{
	decide_and_throttle(PPB, mode, 0);
	return 0;
}
EXPORT_SYMBOL(lbat_set_ppb_mode);

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val > LOW_BATTERY_LEVEL_5) {
		dev_info(dev, "wrong number (%d)\n", val);
		return size;
	}

	dev_info(dev, "your input is %d\n", val);
	decide_and_throttle(UT, val, 0);

	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t low_battery_protect_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_stop=%d\n",
		low_bat_thl_data->low_bat_thl_stop);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1)) {
		dev_info(dev, "stop value not correct\n");
		return size;
	}

	low_bat_thl_data->low_bat_thl_stop = val;
	dev_info(dev, "low_bat_thl_stop=%d\n",
		 low_bat_thl_data->low_bat_thl_stop);
	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "low_bat_thl_level = %d\n",
		low_bat_thl_data->low_bat_thl_level);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);

/*****************************************************************************
 * low battery modify threshold
 ******************************************************************************/
static ssize_t low_battery_modify_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lbat_thd_tbl *thd_info;
	int len = 0, i;

	len += snprintf(buf + len, PAGE_SIZE, "modify enable: %d\n",
		low_bat_thl_data->low_bat_thd_modify);

	for (i = 0; i < low_bat_thl_data->lbat_intr_num; i++) {
		thd_info = &low_bat_thl_data->lbat_thd_info[i][0];
		len += snprintf(buf + len, PAGE_SIZE - len, "volts intr%d: %d %d %d %d\n",
			i + 1, thd_info->thd_volts[LOW_BATTERY_LEVEL_0],
			thd_info->thd_volts[LOW_BATTERY_LEVEL_1],
			thd_info->thd_volts[LOW_BATTERY_LEVEL_2],
			thd_info->thd_volts[LOW_BATTERY_LEVEL_3]);
	}

	return len;
}

static ssize_t low_battery_modify_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int volt_t_size = 0, j = 0;
	unsigned int thd_0, thd_1, thd_2, thd_3;
	unsigned int thd_level[LOW_BATTERY_INIT_NUM] = {0 ,1, 2, 3};
	unsigned int volt_l[LOW_BATTERY_INIT_NUM-1], volt_h[LOW_BATTERY_INIT_NUM-1];
	int intr_no;
	struct lbat_thd_tbl *thd_info;

	if (sscanf(buf, "%u %u %u %u %u\n", &intr_no, &thd_0, &thd_1, &thd_2, &thd_3) != 5) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (thd_0 <= 0 || thd_1 <= 0 || thd_2 <= 0 || thd_3 <= 0 || intr_no < 1 || intr_no > INTR_MAX_NUM) {
		dev_info(dev, "invalid input\n");
		return -EINVAL;
	}

	volt_l[0] = thd_1;
	volt_l[1] = thd_2;
	volt_l[2] = thd_3;
	volt_h[0] = thd_0;
	volt_h[1] = thd_1;
	volt_h[2] = thd_2;

	thd_info = &low_bat_thl_data->lbat_thd_info[intr_no-1][0];
	volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], thd_level,
			LOW_BATTERY_INIT_NUM - 1);

	if (volt_t_size <= 0) {
		dev_notice(dev, "[%s] Failed to rearrange_volt\n", __func__);
		return -ENODATA;
	}

	thd_info->thd_volts_size = volt_t_size;
	for (j = 0; j < volt_t_size; j++)
		thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt_thd;

	dump_thd_volts(dev, thd_info->thd_volts, thd_info->thd_volts_size);

	if (intr_no == 2 && low_bat_thl_data->lbat_intr_num == 2) {
#ifdef LBAT2_ENABLE
		dual_lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[intr_no-1],
			thd_info->thd_volts, thd_info->thd_volts_size);
#endif
	} else if (intr_no == 1) {
		lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[intr_no-1],
			thd_info->thd_volts, thd_info->thd_volts_size);
	}

	low_bat_thl_data->low_bat_thd_modify = 1;
	low_bat_thl_data->temp_cur_stage = 0;
	low_bat_thl_data->temp_reg_stage = 0;
	dev_notice(dev, "modify_enable: %d, temp_cur_stage = %d\n",
				low_bat_thl_data->low_bat_thd_modify,
				low_bat_thl_data->temp_cur_stage);

	return size;
}
static DEVICE_ATTR_RW(low_battery_modify_threshold);

/*****************************************************************************
 * low battery aging factor
 ******************************************************************************/
static ssize_t low_battery_aging_factor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0, i = 0;

	len += snprintf(buf + len, PAGE_SIZE, "aging factor: %d\n",
		low_bat_thl_data->aging_factor);
	len += snprintf(buf + len, PAGE_SIZE, "aging factor volts: ");
	for(i = 0; i < LOW_BATTERY_INIT_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "%dmV ",
		low_bat_thl_data->aging_factor_volts[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE, "\n");

	return len;
}

static ssize_t low_battery_aging_factor_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u\n", &val) != 1) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (val < 0) {
		dev_info(dev, "aging factor not correct\n");
		return size;
	}

	dev_info(dev, "your input aging_factor=%d\n", val);
	apply_aging_factor(val, UT);
	return size;
}
static DEVICE_ATTR_RW(low_battery_aging_factor);

static int lbat_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	if (!low_bat_thl_data)
		return NOTIFY_DONE;

	low_bat_thl_data->psy = v;
	schedule_work(&low_bat_thl_data->temp_work);
	return NOTIFY_DONE;
}

static int check_duplicate(unsigned int *volt_thd)
{
	int i, j;

	for (i = 0; i < LOW_BATTERY_INIT_NUM - 1; i++) {
		for (j = i + 1; j < LOW_BATTERY_INIT_NUM - 1; j++) {
			if (volt_thd[i] == volt_thd[j]) {
				pr_notice("[%s] volt_thd duplicate = %d\n", __func__, volt_thd[i]);
				return -1;
			}
		}
	}
	return 0;
}

static int __used pt_check_power_off(void)
{
	int ret = 0, pt_power_off_lv = LOW_BATTERY_LEVEL_3;
	static int pt_power_off_cnt;
	struct lbat_thd_tbl *thd_info;

	if (!low_bat_thl_data)
		return 0;

	thd_info = &low_bat_thl_data->lbat_thd_info[INTR_1][low_bat_thl_data->temp_reg_stage];
	pt_power_off_lv = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
#ifdef LBAT2_ENABLE
	if (low_bat_thl_data->lbat_intr_num == 2) {
		thd_info =
			&low_bat_thl_data->lbat_thd_info[INTR_2][low_bat_thl_data->temp_reg_stage];
		lbat2_level = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
		pt_power_off_lv = MAX(pt_power_off_lv, lbat2_level)
	}
#endif
	if (!low_bat_thl_data->tag)
		return 0;

	if (low_bat_thl_data->low_bat_thl_level == pt_power_off_lv &&
		low_bat_thl_data->tag->bootmode != KERNEL_POWER_OFF_CHARGING_BOOT) {
		if (pt_power_off_cnt == 0)
			ret = 0;
		else
			ret = 1;
		pt_power_off_cnt++;
		pr_info("[%s] %d ret:%d\n", __func__, pt_power_off_cnt, ret);
	} else
		pt_power_off_cnt = 0;

	if (pt_power_off_cnt >= 4) {
		pr_info("Powering off by PT.\n");
		kernel_power_off();
	}
	return ret;
}

static void __used pt_set_shutdown_condition(void)
{
	static struct power_supply *bat_psy;
	union power_supply_propval prop;
	int ret;

	bat_psy = power_supply_get_by_name("mtk-gauge");
	if (!bat_psy || IS_ERR(bat_psy)) {
		bat_psy = devm_power_supply_get_by_phandle(low_bat_thl_data->dev, "gauge");
		if (!bat_psy || IS_ERR(bat_psy)) {
			pr_info("%s psy is not rdy\n", __func__);
			return;
		}

	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_PRESENT,
					&prop);
	if (!ret && prop.intval == 0) {
		/* gauge enabled */
		prop.intval = 1;
		ret = power_supply_set_property(bat_psy, POWER_SUPPLY_PROP_ENERGY_EMPTY,
						&prop);
		if (ret)
			pr_info("%s fail\n", __func__);
	}
}

static int pt_notify_handler(void *unused)
{
	do {
		wait_event_interruptible(low_bat_thl_data->notify_waiter,
			(low_bat_thl_data->notify_flag == true));

		if (pt_check_power_off()) {
			/* notify battery driver to power off by SOC=0 */
			pt_set_shutdown_condition();
			pr_info("[PT] notify battery SOC=0 to power off.\n");
		}
		mod_timer(&low_bat_thl_data->notify_timer, jiffies + HZ * 20);
		low_bat_thl_data->notify_flag = false;
	} while (!kthread_should_stop());
	return 0;
}

static void pt_timer_func(struct timer_list *t)
{
	low_bat_thl_data->notify_flag = true;
	wake_up_interruptible(&low_bat_thl_data->notify_waiter);
}

int pt_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret = 0, soc = 2;
	struct power_supply *psy = v;
	union power_supply_propval val;

	if (!low_bat_thl_data) {
		pr_info("[%s] low_bat_thl_data not init\n", __func__);
		return NOTIFY_DONE;
	}

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;
	soc = val.intval;
	low_bat_thl_data->lbat_mbrain_info.soc = soc;

	if (soc <= 1 && soc >= 0 && !timer_pending(&low_bat_thl_data->notify_timer)) {
		mod_timer(&low_bat_thl_data->notify_timer, jiffies);
	} else if (soc < 0 || soc > 1) {
		if (timer_pending(&low_bat_thl_data->notify_timer))
			del_timer_sync(&low_bat_thl_data->notify_timer);
	}

	return NOTIFY_DONE;
}

static void pt_notify_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	np = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!np)
		dev_notice(&pdev->dev, "get bootmode fail\n");
	else {
		low_bat_thl_data->tag =
			(struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
		if (!low_bat_thl_data->tag)
			dev_notice(&pdev->dev, "failed to get atag,boot\n");
		else
			dev_notice(&pdev->dev, "bootmode:0x%x\n", low_bat_thl_data->tag->bootmode);
	}

	init_waitqueue_head(&low_bat_thl_data->notify_waiter);
	timer_setup(&low_bat_thl_data->notify_timer, pt_timer_func, TIMER_DEFERRABLE);
	low_bat_thl_data->notify_thread = kthread_run(pt_notify_handler, 0,
					 "pt_notify_thread");
	if (IS_ERR(low_bat_thl_data->notify_thread)) {
		pr_notice("Failed to create notify_thread\n");
		return;
	}
	bp_nb.notifier_call = pt_psy_event;
	ret = power_supply_reg_notifier(&bp_nb);
	if (ret) {
		kthread_stop(low_bat_thl_data->notify_thread);
		pr_notice("power_supply_reg_notifier fail\n");
		return;
	}
}

static void temp_handler(struct work_struct *work)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, temp, temp_stage, temp_thd;
	static int last_temp = MAX_INT;
	bool loop;
	struct lbat_thd_tbl *thd_info, *pre_thd_info;
	int pre_thd_info_level = 0, thd_info_level = 0;
	struct lbat_intr_tbl *pre_lbat_intr_info, *lbat_intr_info;

	if (!low_bat_thl_data)
		return;

	if (!low_bat_thl_data->psy)
		return;

	psy = low_bat_thl_data->psy;

	if (strcmp(psy->desc->name, "battery") != 0)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
#ifdef LBAT2_ENABLE
	temp = val.intval; // because of battery bug, remove me if battery driver fix
#endif
	low_bat_thl_data->lbat_mbrain_info.bat_temp = temp;
	temp_stage = low_bat_thl_data->temp_cur_stage;

	do {
		loop = false;
		if (temp < last_temp) {
			if (temp_stage < low_bat_thl_data->temp_max_stage) {
				temp_thd = low_bat_thl_data->temp_thd[temp_stage];
				if (temp < temp_thd) {
					temp_stage++;
					loop = true;
				}
			}
		} else if (temp > last_temp) {
			if (temp_stage > 0) {
				temp_thd = low_bat_thl_data->temp_thd[temp_stage-1];
				if (temp >= temp_thd) {
					temp_stage--;
					loop = true;
				}
			}
		}
	} while (loop);

	last_temp = temp;

	if (temp_stage <= low_bat_thl_data->temp_max_stage &&
		temp_stage != low_bat_thl_data->temp_cur_stage) {
		if (low_bat_thl_data->ppb_mode != 1 && !low_bat_thl_data->low_bat_thd_modify) {
			pre_thd_info =
				&low_bat_thl_data->lbat_thd_info[INTR_1][low_bat_thl_data->temp_cur_stage];
			thd_info = &low_bat_thl_data->lbat_thd_info[INTR_1][temp_stage];
			low_bat_thl_data->temp_reg_stage = temp_stage;

			pre_lbat_intr_info =
				&pre_thd_info->lbat_intr_info[low_bat_thl_data->pmic_idx[INTR_1]];
			if (pre_lbat_intr_info->ht_en == 1)
				pre_thd_info_level = pre_lbat_intr_info->ht_lv;
			else if (pre_lbat_intr_info->lt_en == 1)
				pre_thd_info_level = pre_lbat_intr_info->lt_lv;

			lbat_intr_info = &thd_info->lbat_intr_info[low_bat_thl_data->pmic_idx[INTR_1]];
			if (lbat_intr_info->ht_en == 1)
				thd_info_level = lbat_intr_info->ht_lv;
			else if (lbat_intr_info->lt_en == 1)
				thd_info_level = lbat_intr_info->lt_lv;

			if (pre_thd_info_level != thd_info_level)
				decide_and_throttle(LBAT_INTR_1, thd_info_level,
					lbat_intr_info->volt_thd);

			lbat_user_modify_thd_ext_locked(low_bat_thl_data->lbat_pt[INTR_1],
				thd_info->thd_volts, thd_info->thd_volts_size);
			dump_thd_volts_ext(thd_info->thd_volts, thd_info->thd_volts_size);
#ifdef LBAT2_ENABLE
			if (low_bat_thl_data->lbat_intr_num == 2) {
				pre_thd_info =
					&low_bat_thl_data->lbat_thd_info[INTR_2][low_bat_thl_data->temp_cur_stage];
				thd_info = &low_bat_thl_data->lbat_thd_info[INTR_2][temp_stage];

				pre_lbat_intr_info =
					&pre_thd_info->lbat_intr_info[low_bat_thl_data->pmic_idx[INTR_2]];
				if (pre_lbat_intr_info->ht_en == 1)
					pre_thd_info_level = pre_lbat_intr_info->ht_lv;
				else if (pre_lbat_intr_info->lt_en == 1)
					pre_thd_info_level = pre_lbat_intr_info->lt_lv;

				lbat_intr_info = &thd_info->lbat_intr_info[low_bat_thl_data->pmic_idx[INTR_2]];
				if (lbat_intr_info->ht_en == 1)
					thd_info_level = lbat_intr_info->ht_lv;
				else if (lbat_intr_info->lt_en == 1)
					thd_info_level = lbat_intr_info->lt_lv;

				if (pre_thd_info_level != thd_info_level)
					decide_and_throttle(LBAT_INTR_2, thd_info_level,
							lbat_intr_info->volt_thd);
				dual_lbat_user_modify_thd_ext_locked(
					low_bat_thl_data->lbat_pt[INTR_2], thd_info->thd_volts,
					thd_info->thd_volts_size);
				dump_thd_volts_ext(thd_info->thd_volts, thd_info->thd_volts_size);
			}
#endif
		}
		low_bat_thl_data->temp_cur_stage = temp_stage;
		low_bat_thl_data->lbat_mbrain_info.temp_stage = temp_stage;
	}
	pr_info("temp=%d, last_temp=%d temp_stage=%d, reg_stage=%d, cur_stage=%d\n",
		temp, last_temp, temp_stage, low_bat_thl_data->temp_reg_stage,
		low_bat_thl_data->temp_cur_stage);
}

static int lvsys_notifier_call(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct lbat_thd_tbl *thd_info;
	int lbat1_level = 0;
#ifdef LBAT2_ENABLE
	int lbat2_level = 0;
#endif

	if (!low_bat_thl_data)
		return NOTIFY_DONE;

	event = event & ~(1 << 15);

	if (event == low_bat_thl_data->lvsys_thd_volt_l) {
		thd_info = &low_bat_thl_data->lbat_thd_info[INTR_1][low_bat_thl_data->temp_reg_stage];
		lbat1_level = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
#ifdef LBAT2_ENABLE
		if (low_bat_thl_data->lbat_intr_num == 2) {
			thd_info = &low_bat_thl_data->lbat_thd_info[INTR_2][low_bat_thl_data->temp_reg_stage];
			lbat2_level = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
			lbat1_level = MAX(lbat1_level, lbat2_level);
		}
#endif
		decide_and_throttle(LVSYS_INTR, lbat1_level, low_bat_thl_data->lvsys_thd_volt_l);
	} else if (event == low_bat_thl_data->lvsys_thd_volt_h)
		decide_and_throttle(LVSYS_INTR, LOW_BATTERY_LEVEL_0,
					low_bat_thl_data->lvsys_thd_volt_h);
	else
		pr_notice("[%s] wrong lvsys thd = %lu\n", __func__, event);

	return NOTIFY_DONE;
}

static struct notifier_block lvsys_notifier = {
	.notifier_call = lvsys_notifier_call,
};

static int fill_thd_info(struct platform_device *pdev, struct low_bat_thl_priv *priv, u32 *volt_thd,
	u32 *thd_level, int intr_no)
{
	int i, j, max_thr_lv, volt_t_size, ret;
	unsigned int volt_l[LOW_BATTERY_LEVEL_NUM-1], volt_h[LOW_BATTERY_LEVEL_NUM-1];
	struct lbat_thd_tbl *thd_info;
	int last_volt_t_size = 0;

	if (intr_no < 0 || intr_no >= INTR_MAX_NUM) {
		dev_notice(&pdev->dev, "[%s] invalid intr_no %d\n", __func__, intr_no);
		return -EINVAL;
	}

	max_thr_lv = LOW_BATTERY_INIT_NUM - 1;

	for (i = 0; i <= priv->temp_max_stage; i++) {
		for (j = 0; j < max_thr_lv; j++) {
			volt_l[j] = volt_thd[i * max_thr_lv + j];
			volt_h[j] = volt_thd[max_thr_lv * (priv->temp_max_stage + 1) +
				i * max_thr_lv + j];
		}

		ret = check_duplicate(volt_l);
		ret |= check_duplicate(volt_h);

		if (ret < 0) {
			dev_notice(&pdev->dev, "[%s] check duplicate error, %d\n", __func__, ret);
			return -EINVAL;
		}

		thd_info = &priv->lbat_thd_info[intr_no][i];
		thd_info->thd_volts_size = max_thr_lv * 2;
		thd_info->lbat_intr_info = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
			sizeof(struct lbat_thd_tbl), GFP_KERNEL);
		thd_info->thd_level_size = LOW_BATTERY_INIT_NUM;
		thd_info->thd_level = devm_kmalloc_array(&pdev->dev, thd_info->thd_level_size,
			sizeof(u32), GFP_KERNEL);
		memcpy(thd_info->thd_level, &thd_level[i * thd_info->thd_level_size],
			sizeof(u32) * thd_info->thd_level_size);
		volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], thd_info->thd_level,
			max_thr_lv);

		if (volt_t_size <= 0) {
			dev_notice(&pdev->dev, "[%s] Failed to rearrange_volt\n", __func__);
			return -ENODATA;
		}

		// different temp stage volt size should be the same
		if (i != 0 && last_volt_t_size != volt_t_size) {
			dev_notice(&pdev->dev,
				"[%s] volt size should be the same, force trigger kernel panic\n",
				__func__);
			BUG_ON(1);
		}
		last_volt_t_size = volt_t_size;

		thd_info->thd_volts_size = volt_t_size;
		thd_info->thd_volts = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
			sizeof(u32), GFP_KERNEL);

		if (!thd_info->thd_volts)
			return -ENOMEM;

		for (j = 0; j < volt_t_size; j++)
			thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt_thd;

		dump_thd_volts(&pdev->dev, thd_info->thd_volts, thd_info->thd_volts_size);
	}

	return 0;
}

static int low_battery_thd_setting(struct platform_device *pdev, struct low_bat_thl_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	char thd_volts_l[THD_VOLTS_LENGTH], thd_volts_h[THD_VOLTS_LENGTH];
	char max_throttle_lv[THD_VOLTS_LENGTH], throttle_lv[THD_VOLTS_LENGTH];
	int ret = 0, bat_type = 0, intr_num = 0;
	int num, i, volt_size, thd_level_size;
	u32 *volt_thd, *thd_level;
	struct device_node *gauge_np = pdev->dev.parent->of_node;

	ret = of_property_read_u32(np, "temperature-max-stage", &priv->temp_max_stage);
	num = of_property_count_u32_elems(np, "temperature-stage-threshold");
	if (ret || num != priv->temp_max_stage) {
		pr_notice("get temperature stage error %d, use 0\n", ret);
		priv->temp_max_stage = 0;
	}

	if (priv->temp_max_stage > 0) {
		priv->temp_thd = devm_kmalloc_array(&pdev->dev, priv->temp_max_stage, sizeof(u32),
			GFP_KERNEL);
		if (!priv->temp_thd)
			return -ENOMEM;
		ret = of_property_read_u32_array(np, "temperature-stage-threshold", priv->temp_thd,
			num);
		if (ret) {
			pr_notice("get temperature-stage-threshold error %d, set stage=0\n", ret);
			priv->temp_max_stage = 0;
		}
	}

	gauge_np = of_find_node_by_name(gauge_np, "mtk-gauge");
	if (!gauge_np)
		pr_notice("[%s] get mtk-gauge node fail\n", __func__);
	else {
		ret = of_property_read_u32(gauge_np, "bat_type", &bat_type);
		if (ret || bat_type < 0 || bat_type >= 10) {
			bat_type = 0;
			dev_notice(&pdev->dev, "[%s] get bat_type fail, ret=%d bat_type=%d\n",
				__func__, ret, bat_type);
		}
	}

	volt_size = (LOW_BATTERY_INIT_NUM - 1) * (priv->temp_max_stage + 1);
	volt_thd = devm_kmalloc_array(&pdev->dev, volt_size * 2, sizeof(u32), GFP_KERNEL);
	if (!volt_thd)
		return -ENOMEM;

	thd_level_size = LOW_BATTERY_INIT_NUM * (priv->temp_max_stage + 1);
	thd_level = devm_kmalloc_array(&pdev->dev, thd_level_size * 2 , sizeof(u32), GFP_KERNEL);
	if (!thd_level)
		return -ENOMEM;

	for (i = 1; i <= INTR_MAX_NUM; i++) {
		ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", bat_type,
			i, VOLT_L_STR);
		if (ret < 0)
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

		num = of_property_count_elems_of_size(np, thd_volts_l, sizeof(u32));
		if (num > 0)
			intr_num++;
		else
			break;
	}

	if (intr_num > 0 && intr_num <= INTR_MAX_NUM) {
		for (i = 0; i < intr_num; i++) {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s",
				bat_type, i + 1, VOLT_L_STR);
			ret |= snprintf(thd_volts_h, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", bat_type,
				i + 1, VOLT_H_STR);
			ret |= snprintf(max_throttle_lv, THD_VOLTS_LENGTH, "lbat%d-max-throttle-level", i);
			ret |= snprintf(throttle_lv, THD_VOLTS_LENGTH, "lbat%d-throttle-level", i);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
				|| of_property_read_u32_array(np, thd_volts_h,
				&volt_thd[volt_size], volt_size)) {
				pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
					thd_volts_l, thd_volts_h);
				return -EINVAL;
			}

			if (of_property_read_u32(np, max_throttle_lv, &priv->max_thd_lv[i]) ||
				of_property_read_u32_array(np, throttle_lv, &thd_level[0], thd_level_size)) {
				pr_info("%s:%d: read thd volt error\n", __func__, __LINE__);
				return -EINVAL;
			}

			ret = fill_thd_info(pdev, priv, volt_thd, thd_level, i);
			if (ret) {
				pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
				return ret;
			} else
				priv->lbat_intr_num++;
		}
	} else {
		if (bat_type > 0) {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "%s-%ds", VOLT_L_STR,
				bat_type + 1);
			ret |= snprintf(thd_volts_h, THD_VOLTS_LENGTH, "%s-%ds", VOLT_H_STR,
				bat_type + 1);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		} else {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "%s", VOLT_L_STR);
			ret |= snprintf(thd_volts_h, THD_VOLTS_LENGTH, "%s", VOLT_H_STR);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
		}

		if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
			|| of_property_read_u32_array(np, thd_volts_h,  &volt_thd[volt_size],
			volt_size)) {
			pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
				thd_volts_l, thd_volts_h);
			return -EINVAL;
		}

		if (of_property_read_u32(np, "lbat1-max-throttle-level", &priv->max_thd_lv[INTR_1]) ||
			of_property_read_u32_array(np, "lbat1-throttle-level", &thd_level[0], thd_level_size)) {
			pr_info("%s:%d: read thd volt error\n", __func__, __LINE__);
			return -EINVAL;
		}

		ret = fill_thd_info(pdev, priv, volt_thd, thd_level, INTR_1);
		if (ret) {
			pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
			return ret;
		} else
			priv->lbat_intr_num++;
	}

	return 0;
}

static int low_battery_aging_setting(struct platform_device *pdev,
		struct low_bat_thl_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	int ret, num = 0;

	num = of_property_count_elems_of_size(np, "aging-factor-volts", sizeof(u32));
	if (num > 0) {
		priv->aging_factor_volts = devm_kmalloc_array(&pdev->dev, LOW_BATTERY_INIT_NUM,
				sizeof(u32), GFP_KERNEL);
		if (!priv->aging_factor_volts)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "aging-factor-volts",
				priv->aging_factor_volts, num);
		if (ret < 0) {
			dev_notice(&pdev->dev, "[%s] failed to get aging-factors ret=%d\n",
					__func__, ret);
			memset(priv->aging_factor_volts, 0, LOW_BATTERY_INIT_NUM * sizeof(unsigned int));
			return -EINVAL;
		}
		dump_aging_factor(priv->aging_factor_volts, num);
	} else {
		dev_notice(&pdev->dev, "[%s] no aging-factors found\n", __func__);
		priv->aging_factor_volts = devm_kmalloc_array(&pdev->dev, LOW_BATTERY_INIT_NUM,
				sizeof(u32), GFP_KERNEL);
		memset(priv->aging_factor_volts, 0, LOW_BATTERY_INIT_NUM * sizeof(unsigned int));
		return -EINVAL;
	}

	return 0;
}

static int low_battery_register_setting(struct platform_device *pdev,
		struct low_bat_thl_priv *priv, enum LOW_BATTERY_INTR_TAG intr_type,
		unsigned int temp_stage)
{
	int ret;
	struct lbat_thd_tbl *thd_info;
	struct lbat_user *lbat_p;

	if (intr_type == INTR_1 && temp_stage <= priv->temp_max_stage) {
		thd_info = &priv->lbat_thd_info[INTR_1][temp_stage];
		priv->lbat_pt[INTR_1] = lbat_user_register_ext("power throttling", thd_info->thd_volts,
		thd_info->thd_volts_size, exec_low_battery_callback);
		lbat_p = priv->lbat_pt[INTR_1];
	} else if (intr_type == INTR_2  && temp_stage <= priv->temp_max_stage) {
#ifdef LBAT2_ENABLE
		thd_info = &priv->lbat_thd_info[INTR_2][temp_stage];
		priv->lbat_pt[INTR_2] = dual_lbat_user_register_ext("power throttling",
			thd_info->thd_volts, thd_info->thd_volts_size, exec_dual_low_battery_callback);
		lbat_p = priv->lbat_pt[INTR_2];
#endif
	} else {
		dev_notice(&pdev->dev, "[%s] invalid intr_type=%d temp_stage=%d\n", __func__,
		intr_type, temp_stage);
		return -1;
	}

	if (IS_ERR(lbat_p)) {
		ret = PTR_ERR(lbat_p);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev, "[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}

	dev_notice(&pdev->dev, "[%s] register intr_type=%d temp_stage=%d done\n", __func__,
		intr_type, temp_stage);

	return 0;
}

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret, i;
	int lvsys_thd_enable, vbat_thd_enable;
	struct low_bat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	INIT_WORK(&priv->temp_work, temp_handler);

	ret = of_property_read_u32(np, "lvsys-thd-enable", &lvsys_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get lvsys-thd-enable ret=%d\n", __func__, ret);
		lvsys_thd_enable = 0;
	}

	ret = of_property_read_u32(np, "vbat-thd-enable", &vbat_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get vbat-thd-enable ret=%d\n", __func__, ret);
		vbat_thd_enable = 1;
	}

	ret = low_battery_aging_setting(pdev, priv);
	if (ret) {
		priv->aging_factor_enable = 0;
		dev_notice(&pdev->dev, "[%s] aging_setting error, ret=%d\n", __func__, ret);
	} else {
		priv->aging_factor_enable = 1;
	}

	if (vbat_thd_enable) {
		ret = low_battery_thd_setting(pdev, priv);
		if (ret) {
			pr_info("[%s] low_battery_thd_setting error, ret=%d\n", __func__, ret);
			return ret;
		}

		for (i = 0; i < priv->lbat_intr_num; i++) {
			ret = low_battery_register_setting(pdev, priv, i, 0);
			if (ret) {
				pr_info("[%s] low_battery_register failed, intr_no=%d ret=%d\n",
					__func__, i, ret);
				return ret;
			}
		}

		if (priv->temp_max_stage > 0) {
			lbat_nb.notifier_call = lbat_psy_event;
			ret = power_supply_reg_notifier(&lbat_nb);
			if (ret) {
				dev_notice(&pdev->dev, "[%s] power_supply_reg_notifier fail\n",
					__func__);
				return ret;
			}
		}
	}

	if (lvsys_thd_enable) {
		ret = of_property_read_u32(np, "lvsys-thd-volt-l", &priv->lvsys_thd_volt_l);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get lvsys-thd-volt-l ret=%d\n", __func__, ret);
			priv->lvsys_thd_volt_l = LVSYS_THD_VOLT_L;
		}

		ret = of_property_read_u32(np, "lvsys-thd-volt-h", &priv->lvsys_thd_volt_h);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get lvsys-thd-volt-h ret=%d\n", __func__, ret);
			priv->lvsys_thd_volt_h = LVSYS_THD_VOLT_H;
		}

		dev_notice(&pdev->dev, "lvsys_register: %d mV, %d mV\n",
			priv->lvsys_thd_volt_l, priv->lvsys_thd_volt_h);

		ret = lvsys_register_notifier(&lvsys_notifier);
		if (ret)
			dev_notice(&pdev->dev, "lvsys_register_notifier error ret=%d\n", ret);
	}

	ret = device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_level);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_modify_threshold);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_aging_factor);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}

	priv->dev = &pdev->dev;
	low_bat_thl_data = priv;
	pt_notify_init(pdev);
	return 0;
}

static const struct of_device_id low_bat_thl_of_match[] = {
	{ .compatible = "mediatek,low_battery_throttling", },
	{ },
};
MODULE_DEVICE_TABLE(of, low_bat_thl_of_match);

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
		.of_match_table = low_bat_thl_of_match,
	},
	.probe = low_battery_throttling_probe,
};

module_platform_driver(low_battery_throttling_driver);
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");
