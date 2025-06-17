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
#define LBAT_PMIC_MAX_LEVEL LOW_BATTERY_LEVEL_3
#define LBAT_PMIC_LEVEL_NUM (LOW_BATTERY_LEVEL_3 + 1)
#define MAX_TABLES 6

struct lbat_intr_tbl {
	unsigned int volt;
	unsigned int ag_volt;
	unsigned int lt_en;
	unsigned int lt_lv;
	unsigned int ht_en;
	unsigned int ht_lv;
};

struct lbat_thd_tbl {
	unsigned int *thd_volts;
	unsigned int *ag_thd_volts;
	int thd_volts_size;
	struct lbat_intr_tbl *lbat_intr_info;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct lbat_thl_priv {
	struct tag_bootmode *tag;
	bool notify_flag;
	struct wait_queue_head notify_waiter;
	struct timer_list notify_timer;
	struct task_struct *notify_thread;
	struct device *dev;
	unsigned int bat_type;
	int lbat_thl_stop;
	int lbat_thd_modify;
	unsigned int lvsys_volt_size;
	unsigned int lvsys_thd_volt_l;
	unsigned int lvsys_thd_volt_h;
	unsigned int ppb_mode;
	unsigned int hpt_mode;
	unsigned int pt_shutdown_en;
	struct work_struct psy_work;
	struct power_supply *psy;
	int *temp_thd;
	int temp_max_stage;
	int temp_cur_stage;
	int temp_reg_stage;
	int *aging_thd;
	int *lvsys_aging_thd;
	int *aging_volts;
	int *lvsys_aging_volts;
	int aging_max_stage;
	int lvsys_aging_max_stage;
	int aging_cur_stage;
	int lvsys_aging_cur_stage;
	unsigned int *aging_factor_volts;
	unsigned int max_thl_lv[INTR_MAX_NUM];
	unsigned int lbat_intr_num;
	unsigned int lbat_lv[INTR_MAX_NUM];    /* charger pmic(vbat) notify level */
	unsigned int l_lbat_lv;                /* largest charger pmic notify level */
	unsigned int lvsys_lv;                 /* main pmic(vsys) notify level */
	unsigned int l_pmic_lv;                /* largest pmic level for charger and main pmic */
	unsigned int *thl_lv;                  /* pmic notify level to throttle level mapping table */
	unsigned int cur_thl_lv;               /* current throttle level to each module */
	unsigned int cur_cg_thl_lv;            /* current cpu/gpu throttle level to each module */
	unsigned int thl_cnt[LOW_BATTERY_USER_NUM][LOW_BATTERY_LEVEL_NUM];
	struct lbat_mbrain lbat_mbrain_info;
	struct lbat_user *lbat_pt[INTR_MAX_NUM];
	struct lbat_thd_tbl lbat_thd[INTR_MAX_NUM][TEMP_MAX_STAGE_NUM];
};

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG, void *data);
	void *data;
};

struct voltage_table {
	u32 low[LBAT_PMIC_MAX_LEVEL * TEMP_MAX_STAGE_NUM];
	u32 high[LBAT_PMIC_MAX_LEVEL * TEMP_MAX_STAGE_NUM];
};

struct multi_voltage_table {
	struct voltage_table tables[MAX_TABLES];
	unsigned int selected_table;
};

struct multi_voltage_table lbat_voltage_table;
struct multi_voltage_table lvsys_voltage_table;
static struct notifier_block lbat_nb;
static struct notifier_block bp_nb;
static struct lbat_thl_priv *lbat_data;
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0}, {0} };
static struct multi_voltage_table *lbat_table_data;
static struct multi_voltage_table *lvsys_table_data;
static low_battery_mbrain_callback lb_mbrain_cb;
static int max_tb_num;
static bool switch_pt;
const char *VOLT_L_STR;
const char *VOLT_H_STR;
static DEFINE_MUTEX(exe_thr_lock);
static int lvsys_thd_enable;
static int vbat_thd_enable;
unsigned int pmic_level_num;
bool lvsys_lv1_trigger;

static unsigned int __used KTF_check_vbat(void)
{
	pr_info("[%s] [%d]\n", __func__, vbat_thd_enable);

	return vbat_thd_enable;
}

static int rearrange_volt(struct lbat_intr_tbl *intr_info, unsigned int *volt_l, unsigned int *volt_h,
	unsigned int num)
{
	unsigned int idx_l = 0, idx_h = 0, idx_t = 0, i;
	unsigned int volt_l_next, volt_h_next;

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
		if (volt_l_next > volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt = volt_l_next;
			intr_info[idx_t].ag_volt = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			idx_l++;
			idx_t++;
		} else if (volt_l_next == volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt = volt_l_next;
			intr_info[idx_t].ag_volt = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_l++;
			idx_h++;
			idx_t++;
		} else if (volt_h_next > 0) {
			intr_info[idx_t].volt = volt_h_next;
			intr_info[idx_t].ag_volt = volt_h_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_h++;
			idx_t++;
		} else
			break;
	}
	for (i = 0; i < idx_t; i++) {
		pr_info("[%s] intr_info[%d] = (v:%d ag_v:%d, trig l[%d %d] h[%d %d])\n",
			__func__, i, intr_info[i].volt, intr_info[i].ag_volt, intr_info[i].lt_en,
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

	r = snprintf(str, len, "mtk_low_battery_throttling: ");
	if (r >= len)
		return;
	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	pr_info("%s Done\n", str);
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

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return 3;
	}

	if (lbat_data->cur_thl_lv && lbcb_tb[prio_val].lbcb) {
		lbcb_tb[prio_val].lbcb(lbat_data->cur_thl_lv, lbcb_tb[prio_val].data);
		pr_info("[%s] notify lv=%d\n", __func__, lbat_data->cur_thl_lv);
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

void exec_throttle(unsigned int thl_level, enum LOW_BATTERY_USER_TAG user, unsigned int thd_volt, unsigned int input)
{
	int i;

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return;
	}

	if (user == HPT && thl_level != lbat_data->cur_thl_lv) {
		for (i = 0; i <= LOW_BATTERY_PRIO_GPU; i++) {
			if (lbcb_tb[i].lbcb)
				lbcb_tb[i].lbcb(thl_level, lbcb_tb[i].data);
		}
		lbat_data->cur_cg_thl_lv = thl_level;
		trace_low_battery_cg_throttling_level(lbat_data->cur_cg_thl_lv);
		pr_info("[%s] [decide_and_throttle] user=%d input=%d thl_lv=%d, volt=%d cur_thl_lv/cg_thl_lv=%d, %d\n",
			__func__, user, input, thl_level, thd_volt, lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);
		return;
	}

	if (lbat_data->cur_thl_lv == thl_level && lbat_data->cur_cg_thl_lv == thl_level) {
		pr_info("[%s] same throttle thl_level=%d\n", __func__, thl_level);
		return;
	}

	lbat_data->lbat_mbrain_info.user = user;
	lbat_data->lbat_mbrain_info.level = thl_level;
	lbat_data->lbat_mbrain_info.thd_volt = thd_volt;
	lbat_data->cur_thl_lv = thl_level;

	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb) {
			if ((lbat_data->hpt_mode > 0 && i > LOW_BATTERY_PRIO_GPU) || !lbat_data->hpt_mode)
				lbcb_tb[i].lbcb(lbat_data->cur_thl_lv, lbcb_tb[i].data);
		}
	}
	trace_low_battery_throttling_level(lbat_data->cur_thl_lv);
	if (!lbat_data->hpt_mode && lbat_data->cur_cg_thl_lv != thl_level) {
		lbat_data->cur_cg_thl_lv = thl_level;
		trace_low_battery_cg_throttling_level(lbat_data->cur_cg_thl_lv);
	}

	if (lb_mbrain_cb)
		lb_mbrain_cb(lbat_data->lbat_mbrain_info);

	lbat_data->thl_cnt[user][thl_level] += 1;

	pr_info("[%s] [decide_and_throttle] user=%d input=%d thl_lv=%d, volt=%d cur_thl_lv/cg_thl_lv=%d, %d\n",
		__func__, user, input, thl_level, thd_volt, lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);

}

static unsigned int convert_to_thl_lv(enum LOW_BATTERY_USER_TAG intr_type, unsigned int temp_stage,
	unsigned int input_lv)
{
	unsigned int thl_lv = input_lv;

	if (!lbat_data || !lbat_data->thl_lv) {
		pr_info("can't find throttle level table, use pmic level=%d\n", thl_lv);
		return thl_lv;
	}

	if (intr_type == LBAT_INTR_1 || intr_type == LBAT_INTR_2) {
		if (input_lv < pmic_level_num  && temp_stage <= lbat_data->temp_max_stage)
			thl_lv = lbat_data->thl_lv[temp_stage * pmic_level_num + input_lv];
		else
			pr_info("%s:Out of boundary: intr_type=%d pmic_lv=%d temp_stage=%d return %d\n", __func__,
				intr_type, input_lv, temp_stage, input_lv);
	} else if (intr_type == LVSYS_INTR) {
		if (input_lv && temp_stage <= lbat_data->temp_max_stage) {
			if (lvsys_lv1_trigger) {
				thl_lv = lbat_data->thl_lv[temp_stage * pmic_level_num + input_lv];
			} else {
				thl_lv = lbat_data->thl_lv[temp_stage * pmic_level_num + (pmic_level_num -
							lbat_data->lvsys_volt_size - 1) + input_lv];
			}
		}
		else if (temp_stage > lbat_data->temp_max_stage)
			pr_info("%s:Out of boundary: intr_type=%d pmic_lv=%d temp_stage=%d return %d\n", __func__,
				intr_type, input_lv, temp_stage, input_lv);
	} else if (intr_type == UT)
		thl_lv = input_lv;

	return thl_lv;
}

/* decide_and_throttle(): for multiple user to trigger PT, arbitrate PT throttle level then execute it
 *   user:
 user want to throttle modules
 *       LBAT_INTR: charger pmic low battery interrupt
 *       LVSYS_INTR: main pmic lvsys interrupt
 *       PPB: ppb module to enable/disable PT
 *       UT: user command to force throttle level
 *   input: user input parameters
 *       for LBAT_INTR and LVSYS_INTR: pmic notify level (lbat 0~3, lvsys 0~1)
 *       for PPB: ppb mode
 *       for UT: throttle level
 */
static int __used decide_and_throttle(enum LOW_BATTERY_USER_TAG user, unsigned int input, unsigned int thd_volt)
{
	struct lbat_thd_tbl *thd_info[INTR_MAX_NUM];
	unsigned int low_thd_volts[LBAT_PMIC_LEVEL_NUM] = {MIN_LBAT_VOLT+40, MIN_LBAT_VOLT+30, MIN_LBAT_VOLT+20,
		MIN_LBAT_VOLT+10};
	int temp_cur_stage = 0;
	unsigned int lbat_thl_lv = 0, lvsys_thl_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -ENODATA;
	}

	mutex_lock(&exe_thr_lock);
	if (user == LBAT_INTR_1 || user == LBAT_INTR_2) {
		if (user == LBAT_INTR_1)
			lbat_data->lbat_lv[INTR_1] = input;
		else if (user == LBAT_INTR_2)
			lbat_data->lbat_lv[INTR_2] = input;

		lbat_data->l_lbat_lv = MAX(lbat_data->lbat_lv[INTR_1], lbat_data->lbat_lv[INTR_2]);

		if (lbat_data->lbat_thl_stop > 0 || lbat_data->ppb_mode == 1) {
			pr_info("[%s] user=%d input=%d not apply, stop/ppb=%d/%d\n", __func__,
				user, input, lbat_data->lbat_thl_stop, lbat_data->ppb_mode);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			if (lvsys_lv1_trigger) {
				lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv, lbat_data->lvsys_lv ?
							(lbat_data->lvsys_lv):0);
			} else {
				lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv, lbat_data->lvsys_lv ?
					(pmic_level_num - lbat_data->lvsys_volt_size + lbat_data->lvsys_lv - 1):0);
			}
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == LVSYS_INTR) {
		lbat_data->lvsys_lv = input;
		if (lbat_data->lbat_thl_stop > 0 || lbat_data->ppb_mode == 1) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d, ppb_mode=%d\n", __func__,
				user, input, lbat_data->lbat_thl_stop, lbat_data->ppb_mode);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			if (lvsys_lv1_trigger) {
				lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv, lbat_data->lvsys_lv ?
							(lbat_data->lvsys_lv):0);
			} else {
				lbat_data->l_pmic_lv = MAX(lbat_data->l_lbat_lv, lbat_data->lvsys_lv ?
					(pmic_level_num - lbat_data->lvsys_volt_size + lbat_data->lvsys_lv - 1):0);
			}
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
		}
		mutex_unlock(&exe_thr_lock);
	} else if (user == PPB) {
		lbat_data->ppb_mode = input;
		if (lbat_data->lbat_thl_stop > 0) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d\n", __func__, user, input,
				lbat_data->lbat_thl_stop);
			mutex_unlock(&exe_thr_lock);
		} else if (lbat_data->ppb_mode == 1) {
			exec_throttle(LOW_BATTERY_LEVEL_0, user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1], &low_thd_volts[0],
				LBAT_PMIC_LEVEL_NUM);
#ifdef LBAT2_ENABLE
			if (lbat_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_2], &low_thd_volts[0],
					LBAT_PMIC_LEVEL_NUM);
			}
#endif
			dump_thd_volts_ext(&low_thd_volts[0], LBAT_PMIC_LEVEL_NUM);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
			temp_cur_stage = lbat_data->temp_cur_stage;
			thd_info[INTR_1] = &lbat_data->lbat_thd[INTR_1][temp_cur_stage];
			thd_info[INTR_2] = &lbat_data->lbat_thd[INTR_2][temp_cur_stage];
			lbat_data->temp_reg_stage = temp_cur_stage;
			mutex_unlock(&exe_thr_lock);
			lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1],
				thd_info[INTR_1]->ag_thd_volts, thd_info[INTR_1]->thd_volts_size);
			dump_thd_volts_ext(thd_info[INTR_1]->ag_thd_volts, thd_info[INTR_1]->thd_volts_size);
#ifdef LBAT2_ENABLE
			if (lbat_data->lbat_intr_num == 2) {
				dual_lbat_user_modify_thd_ext_locked( lbat_data->lbat_pt[INTR_2],
					thd_info[INTR_2]->ag_thd_volts, thd_info[INTR_2]->thd_volts_size);
				dump_thd_volts_ext(thd_info[INTR_2]->ag_thd_volts, thd_info[INTR_2]->thd_volts_size);
			}
#endif
		}
	} else if (user == HPT) {
		lbat_data->hpt_mode = input;
		if (lbat_data->lbat_thl_stop > 0) {
			pr_info("[%s] user=%d input=%d not apply, stop=%d\n", __func__, user, input,
				lbat_data->lbat_thl_stop);
			mutex_unlock(&exe_thr_lock);
		} else if (lbat_data->hpt_mode > 0) {
			exec_throttle(LOW_BATTERY_LEVEL_0, user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
		} else {
			lbat_thl_lv = convert_to_thl_lv(LBAT_INTR_1, lbat_data->temp_cur_stage, lbat_data->l_lbat_lv);
			lvsys_thl_lv = convert_to_thl_lv(LVSYS_INTR, lbat_data->temp_cur_stage, lbat_data->lvsys_lv);
			exec_throttle(MAX(lbat_thl_lv, lvsys_thl_lv), user, thd_volt, input);
			mutex_unlock(&exe_thr_lock);
		}
	} else if (user == UT) {
		lbat_data->lbat_thl_stop = 1;
		exec_throttle(input, user, thd_volt, input);
		mutex_unlock(&exe_thr_lock);
	} else {
		mutex_unlock(&exe_thr_lock);
	}

	return 0;
}

static int lbat_thd_to_lv(unsigned int thd, unsigned int temp_stage, enum LOW_BATTERY_USER_TAG intr_type)
{
	unsigned int i, level = 0;
	struct lbat_intr_tbl *info;
	struct lbat_thd_tbl *thd_info;

	if (!lbat_data)
		return 0;

	if (intr_type == LBAT_INTR_1)
		thd_info = &lbat_data->lbat_thd[INTR_1][temp_stage];
	else if (intr_type == LBAT_INTR_2)
		thd_info = &lbat_data->lbat_thd[INTR_2][temp_stage];
	else if (intr_type == LVSYS_INTR)
		thd_info = &lbat_data->lbat_thd[LVSYS_INT][temp_stage];
	else {
		pr_notice("[%s] wrong intr_type=%d\n", __func__, intr_type);
		return -1;
	}

	for (i = 0; i < thd_info->thd_volts_size; i++) {
		info = &thd_info->lbat_intr_info[i];
		if (thd == thd_info->ag_thd_volts[i]) {
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

static int get_aging_stage(enum LOW_BATTERY_INTR_TAG user)
{
	int cycle = 0, i, max_stage;
	struct power_supply *psy;
	union power_supply_propval val;
	const int *aging_thd;

	if (!lbat_data)
		return 0;

	if (!lbat_data->psy)
		return 0;

	psy = lbat_data->psy;

	if (strcmp(psy->desc->name, "battery") != 0)
		return 0;

	if (user == LVSYS_INT) {
		max_stage = lbat_data->lvsys_aging_max_stage;
		aging_thd = lbat_data->lvsys_aging_thd;
	} else {
		max_stage = lbat_data->aging_max_stage;
		aging_thd = lbat_data->aging_thd;
	}

	if (max_stage <= 0)
		return 0;

	if (power_supply_get_property(psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val) == 0)
		cycle = val.intval;

	for (i = 0; i < max_stage; i++) {
		if (cycle < aging_thd[i])
			break;
	}

	return i;
}

static void update_thresholds(int temp_stage, int aging_stage, int lvsys_aging_stage)
{
	struct lbat_thd_tbl *thd_info;
	unsigned int ag_offset, lvsys_ag_offset;
	int i;
	u32 *lvsys_volt_thd;

	if(!lvsys_table_data || !lbat_data) {
		pr_info("[%s] lvsys/lbat data not init\n", __func__);
		return;
	}

	if (aging_stage == 0)
		ag_offset = 0;
	else
		ag_offset = lbat_data->aging_volts[lbat_data->aging_max_stage * temp_stage + aging_stage - 1];
	if (lvsys_aging_stage == 0)
		lvsys_ag_offset = 0;
	else
		lvsys_ag_offset = lbat_data->lvsys_aging_volts[lbat_data->lvsys_aging_max_stage *
									temp_stage + lvsys_aging_stage - 1];

	thd_info = &lbat_data->lbat_thd[INTR_1][temp_stage];
	mutex_lock(&exe_thr_lock);
	for (i = 0; i < thd_info->thd_volts_size; i++) {
		thd_info->ag_thd_volts[i] = thd_info->thd_volts[i] + ag_offset;
		thd_info->lbat_intr_info[i].ag_volt = thd_info->lbat_intr_info[i].volt + ag_offset;
	}
	mutex_unlock(&exe_thr_lock);

	lbat_data->temp_reg_stage = temp_stage;
	lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1],
		thd_info->ag_thd_volts, thd_info->thd_volts_size);
	dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);

	if (lvsys_thd_enable && lbat_data->lvsys_volt_size > 1) {
		thd_info = &lbat_data->lbat_thd[LVSYS_INT][temp_stage];
		lvsys_volt_thd = kmalloc_array(lbat_data->lvsys_volt_size * 2, sizeof(u32), GFP_KERNEL);
		if(!lvsys_volt_thd)
			return;
		mutex_lock(&exe_thr_lock);
		for (i = 0; i < lbat_data->lvsys_volt_size; i++) {
			lvsys_volt_thd[i * 2] = lvsys_table_data->tables[lvsys_table_data->selected_table].high[
				temp_stage * lbat_data->lvsys_volt_size + i] + lvsys_ag_offset;
			lvsys_volt_thd[i * 2 + 1] = lvsys_table_data->tables[lvsys_table_data->selected_table].low[
				temp_stage * lbat_data->lvsys_volt_size + i] + lvsys_ag_offset;
		}
		for (i = 0; i < lbat_data->lvsys_volt_size * 2; i++) {
			thd_info->ag_thd_volts[i] = thd_info->thd_volts[i] + lvsys_ag_offset;
			thd_info->lbat_intr_info[i].ag_volt = thd_info->lbat_intr_info[i].volt + lvsys_ag_offset;
		}
		mutex_unlock(&exe_thr_lock);
		lvsys_modify_thd(lvsys_volt_thd, lbat_data->lvsys_volt_size * 2);
		kfree(lvsys_volt_thd);
		dump_lvsys_thd();
	}
#ifdef LBAT2_ENABLE
	if (lbat_data->lbat_intr_num == 2) {
		thd_info = &lbat_data->lbat_thd[INTR_2][temp_stage];
		if (aging_stage == 0)
			ag_offset = 0;
		else
			ag_offset = lbat_data->aging_volts[lbat_data->aging_max_stage * temp_stage + aging_stage - 1];
		mutex_lock(&exe_thr_lock);
		for (i = 0; i < thd_info->thd_volts_size; i++) {
			thd_info->ag_thd_volts[i] = thd_info->thd_volts[i] + ag_offset;
			thd_info->lbat_intr_info[i].ag_volt = thd_info->lbat_intr_info[i].volt + ag_offset;
		}
		mutex_unlock(&exe_thr_lock);
		dual_lbat_user_modify_thd_ext_locked(
			lbat_data->lbat_pt[INTR_2], thd_info->ag_thd_volts,
			thd_info->thd_volts_size);
		dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);
	}
#endif
	lbat_data->temp_cur_stage = temp_stage;
	lbat_data->lbat_mbrain_info.temp_stage = temp_stage;
	lbat_data->aging_cur_stage = aging_stage;
	lbat_data->lvsys_aging_cur_stage = lvsys_aging_stage;
}

void exec_low_battery_callback(unsigned int thd)
{
	unsigned int lbat_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not allocate\n", __func__);
		return;
	}

	lbat_lv = lbat_thd_to_lv(thd, lbat_data->temp_reg_stage, LBAT_INTR_1);
	if (lbat_lv == -1)
		return;

	decide_and_throttle(LBAT_INTR_1, lbat_lv, thd);
}

void exec_dual_low_battery_callback(unsigned int thd)
{
	unsigned int lbat_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not allocate\n", __func__);
		return;
	}

	lbat_lv = lbat_thd_to_lv(thd, lbat_data->temp_reg_stage, LBAT_INTR_2);
	if (lbat_lv == -1)
		return;

	decide_and_throttle(LBAT_INTR_2, lbat_lv, thd);
}

int lbat_set_ppb_mode(unsigned int mode)
{
	decide_and_throttle(PPB, mode, 0);
	return 0;
}
EXPORT_SYMBOL(lbat_set_ppb_mode);

int lbat_set_hpt_mode(unsigned int enable)
{
	decide_and_throttle(HPT, enable, 0);
	return 0;
}
EXPORT_SYMBOL(lbat_set_hpt_mode);

/*****************************************************************************
 * low battery throttle cnt
 ******************************************************************************/
static ssize_t low_battery_throttle_cnt_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	unsigned int len = 0;
	int i = 0, j = 0;
	int user[] = {LBAT_INTR_1, LVSYS_INTR};


	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(user); i++) {
		len += snprintf(buf + len, PAGE_SIZE, "user%d: ", i);
		for ( j = 0; j < LOW_BATTERY_LEVEL_NUM; j++)
			len += snprintf(buf + len, PAGE_SIZE, "%d ", lbat_data->thl_cnt[i][j]);
	}

	return len;
}

static DEVICE_ATTR_RO(low_battery_throttle_cnt);

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "cur_thl_lv=%d\n", lbat_data->cur_thl_lv);
	return snprintf(buf, PAGE_SIZE, "%u\n", lbat_data->cur_thl_lv);
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
	dev_dbg(dev, "lbat_thl_stop=%d\n", lbat_data->lbat_thl_stop);
	return snprintf(buf, PAGE_SIZE, "%u\n", lbat_data->lbat_thl_stop);
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

	lbat_data->lbat_thl_stop = val;
	dev_info(dev, "lbat_thl_stop=%d\n", lbat_data->lbat_thl_stop);
	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "cur_thl_lv=%d cur_cg_thl_lv=%d\n", lbat_data->cur_thl_lv, lbat_data->cur_cg_thl_lv);
	return snprintf(buf, PAGE_SIZE, "%u, %u\n", lbat_data->cur_thl_lv,lbat_data->cur_cg_thl_lv);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "cur_thl_lv=%d\n", lbat_data->cur_thl_lv);
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
	int len = 0, i, j;

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE, "modify enable: %d\n", lbat_data->lbat_thd_modify);

	for (j = 0; j <= lbat_data->temp_max_stage; j++) {
		for (i = 0; i < lbat_data->lbat_intr_num; i++) {
			thd_info = &lbat_data->lbat_thd[i][j];
			len += snprintf(buf + len, PAGE_SIZE - len, "volts intr%d: %d %d %d %d\n",
				i + 1, thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_0],
				thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_1],
				thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_2],
				thd_info->ag_thd_volts[LOW_BATTERY_LEVEL_3]);
		}
	}
	return len;
}

static ssize_t low_battery_modify_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int volt_t_size = 0, j = 0, i = 0;
	unsigned int thd_0[4], thd_1[4], thd_2[4], thd_3[4];
	unsigned int volt_l[LBAT_PMIC_MAX_LEVEL], volt_h[LBAT_PMIC_MAX_LEVEL];
	int intr_no;
	struct lbat_thd_tbl *thd_info;
	int aging_stage, lvsys_aging_stage;
	u32 thd_volts_tbl[12];

	if (!lbat_data) {
		pr_info("[%s] Failed to create lbat_data\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		&intr_no, &thd_0[0], &thd_1[0], &thd_2[0], &thd_3[0], &thd_0[1], &thd_1[1], &thd_2[1], &thd_3[1],
		&thd_0[2], &thd_1[2], &thd_2[2], &thd_3[2], &thd_0[3], &thd_1[3], &thd_2[3], &thd_3[3]) != 17) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (thd_0[0] <= 0 || thd_1[0] <= 0 || thd_2[0] <= 0 || thd_3[0] <= 0 ||
		thd_0[1] <= 0 || thd_1[1] <= 0 || thd_2[1] <= 0 || thd_3[1] <= 0 ||
		thd_0[2] <= 0 || thd_1[2] <= 0 || thd_2[2] <= 0 || thd_3[2] <= 0 ||
		thd_0[3] <= 0 || thd_1[3] <= 0 || thd_2[3] <= 0 || thd_3[3] <= 0 ||
		intr_no < 1 || intr_no > INTR_MAX_NUM || intr_no != lbat_data->lbat_intr_num) {
		dev_info(dev, "invalid input\n");
		return -EINVAL;
	}
	for (i = 0; i <= lbat_data->temp_max_stage; i++) {
		volt_l[0] = thd_1[i];
		volt_l[1] = thd_2[i];
		volt_l[2] = thd_3[i];
		volt_h[0] = thd_0[i];
		volt_h[1] = thd_1[i];
		volt_h[2] = thd_2[i];
		mutex_lock(&exe_thr_lock);

		thd_info = &lbat_data->lbat_thd[intr_no-1][i];
		volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], LBAT_PMIC_MAX_LEVEL);

		if (volt_t_size <= 0) {
			dev_notice(dev, "[%s] Failed to rearrange_volt\n", __func__);
			mutex_unlock(&exe_thr_lock);
			return -ENODATA;
		}

		thd_info->thd_volts_size = volt_t_size;
		for (j = 0; j < volt_t_size; j++) {
			thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt;
			thd_info->ag_thd_volts[j] = thd_info->lbat_intr_info[j].ag_volt;
			dev_notice(dev, "j:%d, thd_volts: %d, ag_thd_volts:%d\n", j,
				thd_info->thd_volts[j], thd_info->ag_thd_volts[j]);
		}
		mutex_unlock(&exe_thr_lock);

		dump_thd_volts(dev, thd_info->ag_thd_volts, thd_info->thd_volts_size);
	}

	if (switch_pt){
		// Fill thd_volts_tbx_l
		for (i = 0; i < 4; i++) {
			thd_volts_tbl[i*3] = thd_1[i];
			thd_volts_tbl[i*3 + 1] = thd_2[i];
			thd_volts_tbl[i*3 + 2] = thd_3[i];
		}
		memcpy(lbat_table_data->tables[lbat_table_data->selected_table].low,
			thd_volts_tbl, sizeof(thd_volts_tbl));

		// Fill thd_volts_tbx_h
		for (i = 0; i < 4; i++) {
			thd_volts_tbl[i*3] = thd_0[i];
			thd_volts_tbl[i*3 + 1] = thd_1[i];
			thd_volts_tbl[i*3 + 2] = thd_2[i];
		}
		memcpy(lbat_table_data->tables[lbat_table_data->selected_table].high,
			thd_volts_tbl, sizeof(thd_volts_tbl));
	}

	aging_stage = get_aging_stage(INTR_1);
	lvsys_aging_stage = get_aging_stage(LVSYS_INT);
	thd_info = &lbat_data->lbat_thd[intr_no-1][lbat_data->temp_reg_stage];

	dev_notice(dev, "temp_stage: %d, aging_stage: %d\n", lbat_data->temp_reg_stage, aging_stage);

	if ((lbat_data->temp_reg_stage <= lbat_data->temp_max_stage) ||
	(aging_stage <= lbat_data->aging_max_stage) ||
	(lvsys_aging_stage <= lbat_data->lvsys_aging_max_stage)) {
		if (!lbat_data->lbat_thd_modify)
			update_thresholds(lbat_data->temp_reg_stage, aging_stage, lvsys_aging_stage);
	}

	lbat_data->lbat_thd_modify = 0;
	dev_notice(dev, "modify_enable: %d, temp_cur_stage = %d\n", lbat_data->lbat_thd_modify,
		lbat_data->temp_cur_stage);

	return size;
}
static DEVICE_ATTR_RW(low_battery_modify_threshold);

/*****************************************************************************
 * lvsys modify threshold
 ******************************************************************************/
static ssize_t lvsys_modify_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0, i, j;

	if(!lvsys_table_data || !lbat_data) {
		pr_info("[%s] lvsys/lbat data not init\n", __func__);
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE, "LVSYS table_idx: %d\n", lvsys_table_data->selected_table);

	for (i = 0; i <= lbat_data->temp_max_stage; i++) {
		len += snprintf(buf + len, PAGE_SIZE, "temp stage %d:", i);
		for (j = 0; j < lbat_data->lvsys_volt_size; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len, " %d", lvsys_table_data->tables[
				lvsys_table_data->selected_table].high[i * lbat_data->lvsys_volt_size + j]);
			len += snprintf(buf + len, PAGE_SIZE - len, " %d", lvsys_table_data->tables[
				lvsys_table_data->selected_table].low[i * lbat_data->lvsys_volt_size + j]);
		}
		len += snprintf(buf + len, PAGE_SIZE, "\n");
	}
	return len;
}

static ssize_t lvsys_modify_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int volt_t_size = 0, j = 0, i = 0, len = 0;
	u32 volt_l[LBAT_PMIC_MAX_LEVEL * TEMP_MAX_STAGE_NUM], volt_h[LBAT_PMIC_MAX_LEVEL * TEMP_MAX_STAGE_NUM];
	unsigned int table_idx;
	int aging_stage, lvsys_aging_stage;
	struct lbat_thd_tbl *thd_info;

	if (switch_pt == false){
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if (sscanf(buf, "%u%n", &table_idx, &len) != 1) {
		dev_info(dev, "Failed to read table_idx\n");
		return -EINVAL;
	}
	buf += len;
	if (table_idx > max_tb_num) {
		dev_info(dev, "Invalid table_idx: %u\n", table_idx);
		return -EINVAL;
	}
	for (i = 0; i < lbat_data->lvsys_volt_size * (lbat_data->temp_max_stage + 1); i++){
		if (sscanf(buf, "%u%n", &volt_h[i], &len) != 1) {
			dev_info(dev, "Failed to read lvsys voltage %d\n", i * 2);
			return -EINVAL;
		}
		buf += len;
		if (sscanf(buf, "%u%n", &volt_l[i], &len) != 1) {
			dev_info(dev, "Failed to read lvsys voltage %d\n", i * 2 + 1);
			return -EINVAL;
		}
		buf += len;
	}

	for (i = 0; i <= lbat_data->temp_max_stage; i++) {
		mutex_lock(&exe_thr_lock);

		thd_info = &lbat_data->lbat_thd[LVSYS_INT][i];
		volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0],
						lbat_data->lvsys_volt_size);

		if (volt_t_size <= 0) {
			dev_notice(dev, "[%s] Failed to rearrange_volt\n", __func__);
			mutex_unlock(&exe_thr_lock);
			return -ENODATA;
		}

		thd_info->thd_volts_size = volt_t_size;
		for (j = 0; j < volt_t_size; j++) {
			thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt;
			thd_info->ag_thd_volts[j] = thd_info->lbat_intr_info[j].ag_volt;
			dev_notice(dev, "j:%d, thd_volts: %d, ag_thd_volts:%d\n", j,
				thd_info->thd_volts[j], thd_info->ag_thd_volts[j]);
		}
		mutex_unlock(&exe_thr_lock);
	}

	if (switch_pt){
		// Fill thd_volts_tbx_l
		memcpy(lvsys_table_data->tables[table_idx].low, volt_l, lbat_data->lvsys_volt_size *
			(lbat_data->temp_max_stage + 1) * sizeof(u32));
		// Fill thd_volts_tbx_h
		memcpy(lvsys_table_data->tables[table_idx].high, volt_h, lbat_data->lvsys_volt_size *
			(lbat_data->temp_max_stage + 1) * sizeof(u32));
	}

	aging_stage = get_aging_stage(INTR_1);
	lvsys_aging_stage = get_aging_stage(LVSYS_INT);
	thd_info = &lbat_data->lbat_thd[LVSYS_INT][lbat_data->temp_reg_stage];

	dev_notice(dev, "temp_stage: %d, aging_stage: %d\n", lbat_data->temp_reg_stage, aging_stage);

	if ((lbat_data->temp_reg_stage <= lbat_data->temp_max_stage) ||
	(aging_stage <= lbat_data->aging_max_stage) ||
	(lvsys_aging_stage <= lbat_data->lvsys_aging_max_stage)) {
		if (table_idx == lvsys_table_data->selected_table)
			update_thresholds(lbat_data->temp_reg_stage, aging_stage, lvsys_aging_stage);
	}

	return size;
}
static DEVICE_ATTR_RW(lvsys_modify_threshold);

static int lbat_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	if (!lbat_data)
		return NOTIFY_DONE;

	lbat_data->psy = v;
	schedule_work(&lbat_data->psy_work);
	return NOTIFY_DONE;
}

static int check_duplicate(unsigned int *volt_thd, enum LOW_BATTERY_USER_TAG user, struct lbat_thl_priv *priv)
{
	int i, j;
	int pmic_max_level = 0;

	if (user == LBAT_INTR_1 || user == LBAT_INTR_2)
		pmic_max_level = LBAT_PMIC_MAX_LEVEL;
	else if (user == LVSYS_INTR)
		pmic_max_level = priv->lvsys_volt_size;

	for (i = 0; i < pmic_max_level; i++) {
		for (j = i + 1; j < pmic_max_level; j++) {
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
	int ret = 0, pt_power_off_lv = LBAT_PMIC_MAX_LEVEL;
	static int pt_power_off_cnt;
	struct lbat_thd_tbl *thd_info;

	if (!lbat_data)
		return 0;
	if (vbat_thd_enable==1){
		thd_info = &lbat_data->lbat_thd[INTR_1][lbat_data->temp_reg_stage];
		pt_power_off_lv = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
	} else {
		pt_power_off_lv = lbat_data->lvsys_volt_size;
	}
#ifdef LBAT2_ENABLE
	if (lbat_data->lbat_intr_num == 2) {
		thd_info =
			&lbat_data->lbat_thd[INTR_2][lbat_data->temp_reg_stage];
		lbat2_level = thd_info->lbat_intr_info[thd_info->thd_volts_size - 1].lt_lv;
		pt_power_off_lv = MAX(pt_power_off_lv, lbat2_level)
	}
#endif
	if (!lbat_data->tag)
		return 0;

	if (lbat_data->cur_thl_lv == pt_power_off_lv &&
		lbat_data->tag->bootmode != KERNEL_POWER_OFF_CHARGING_BOOT) {
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
		hw_protection_shutdown("Power Off by PT", 10000);
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
		bat_psy = devm_power_supply_get_by_phandle(lbat_data->dev, "gauge");
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
		wait_event_interruptible(lbat_data->notify_waiter,
			(lbat_data->notify_flag == true));

		if (pt_check_power_off()) {
			/* notify battery driver to power off by SOC=0 */
			pt_set_shutdown_condition();
			pr_info("[PT] notify battery SOC=0 to power off.\n");
		}
		mod_timer(&lbat_data->notify_timer, jiffies + HZ * 20);
		lbat_data->notify_flag = false;
	} while (!kthread_should_stop());
	return 0;
}

static void pt_timer_func(struct timer_list *t)
{
	lbat_data->notify_flag = true;
	wake_up_interruptible(&lbat_data->notify_waiter);
}

int pt_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret = 0, soc = 2;
	struct power_supply *psy = v;
	union power_supply_propval val;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not init\n", __func__);
		return NOTIFY_DONE;
	}

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;
	soc = val.intval;
	lbat_data->lbat_mbrain_info.soc = soc;

	if (lbat_data->pt_shutdown_en) {
		if (soc <= 1 && soc >= 0 && !timer_pending(&lbat_data->notify_timer)) {
			mod_timer(&lbat_data->notify_timer, jiffies);
		} else if (soc < 0 || soc > 1) {
			if (timer_pending(&lbat_data->notify_timer))
				del_timer_sync(&lbat_data->notify_timer);
		}
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
		lbat_data->tag =
			(struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
		if (!lbat_data->tag)
			dev_notice(&pdev->dev, "failed to get atag,boot\n");
		else
			dev_notice(&pdev->dev, "bootmode:0x%x\n", lbat_data->tag->bootmode);
	}

	init_waitqueue_head(&lbat_data->notify_waiter);
	timer_setup(&lbat_data->notify_timer, pt_timer_func, TIMER_DEFERRABLE);
	lbat_data->notify_thread = kthread_run(pt_notify_handler, 0,
					 "pt_notify_thread");
	if (IS_ERR(lbat_data->notify_thread)) {
		pr_notice("Failed to create notify_thread\n");
		return;
	}
	bp_nb.notifier_call = pt_psy_event;
	ret = power_supply_reg_notifier(&bp_nb);
	if (ret) {
		kthread_stop(lbat_data->notify_thread);
		pr_notice("power_supply_reg_notifier fail\n");
		return;
	}
}

static void psy_handler(struct work_struct *work)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, temp, temp_stage, temp_thd, aging_stage = 0, lvsys_aging_stage = 0;
	static int last_temp = MAX_INT;
	bool loop;
	unsigned int pre_thl_lv, cur_thl_lv, thl_lv_idx;

	if (!lbat_data)
		return;

	if (!lbat_data->psy)
		return;

	psy = lbat_data->psy;

	if (strcmp(psy->desc->name, "battery") != 0)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
#ifdef LBAT2_ENABLE
	temp = val.intval; // because of battery bug, remove me if battery driver fix
#endif
	lbat_data->lbat_mbrain_info.bat_temp = temp;
	temp_stage = lbat_data->temp_cur_stage;

	do {
		loop = false;
		if (temp < last_temp) {
			if (temp_stage < lbat_data->temp_max_stage) {
				temp_thd = lbat_data->temp_thd[temp_stage];
				if (temp < temp_thd) {
					temp_stage++;
					loop = true;
				}
			}
		} else if (temp > last_temp) {
			if (temp_stage > 0) {
				temp_thd = lbat_data->temp_thd[temp_stage-1];
				if (temp >= temp_thd) {
					temp_stage--;
					loop = true;
				}
			}
		}
	} while (loop);

	last_temp = temp;

	aging_stage = get_aging_stage(INTR_1);
	lvsys_aging_stage = get_aging_stage(LVSYS_INT);

	if ((temp_stage <= lbat_data->temp_max_stage && temp_stage != lbat_data->temp_cur_stage) ||
		(aging_stage <= lbat_data->aging_max_stage && aging_stage != lbat_data->aging_cur_stage) ||
		(lvsys_aging_stage <= lbat_data->lvsys_aging_max_stage && lvsys_aging_stage !=
									lbat_data->lvsys_aging_cur_stage)) {
		pr_info("[%s]: temp stage = %d, temp_cur_stage = %d, aging_stage = %d, aging_cur_stage = %d, lvsys_aging_stage = %d, lvsys_aging_cur_stage = %d"
			, __func__, temp_stage, lbat_data->temp_cur_stage, aging_stage
			, lbat_data->aging_cur_stage, lvsys_aging_stage, lbat_data->lvsys_aging_cur_stage);
		if (lbat_data->ppb_mode != 1 && !lbat_data->lbat_thd_modify) {
			thl_lv_idx = lbat_data->temp_cur_stage * pmic_level_num + lbat_data->l_pmic_lv;
			pre_thl_lv = lbat_data->thl_lv[thl_lv_idx];
			thl_lv_idx = temp_stage * pmic_level_num + lbat_data->l_pmic_lv;
			cur_thl_lv = lbat_data->thl_lv[thl_lv_idx];
			if (pre_thl_lv != cur_thl_lv) {
				lbat_data->temp_cur_stage = temp_stage;
				lbat_data->aging_cur_stage = aging_stage;
				lbat_data->lvsys_aging_cur_stage = lvsys_aging_stage;
				if (vbat_thd_enable)
					decide_and_throttle(LBAT_INTR_1, lbat_data->lbat_lv[INTR_1], 0);
				else
					decide_and_throttle(LVSYS_INTR, lbat_data->lvsys_lv, 0);
			}
			update_thresholds(temp_stage, aging_stage, lvsys_aging_stage);
		}
		lbat_data->temp_cur_stage = temp_stage;
		lbat_data->lbat_mbrain_info.temp_stage = temp_stage;
		lbat_data->aging_cur_stage = aging_stage;
		lbat_data->lvsys_aging_cur_stage = lvsys_aging_stage;
	}
}

static int lvsys_notifier_call(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	unsigned int lbat_lv = 0;

	if (!lbat_data) {
		pr_info("[%s] lbat_data not allocate\n", __func__);
		return NOTIFY_DONE;
	}
	event = event & ~(1 << 15);

	if (lbat_data->lvsys_volt_size > 1) {
		lbat_lv = lbat_thd_to_lv(event, lbat_data->temp_reg_stage, LVSYS_INTR);
		if (lbat_lv == -1)
			return NOTIFY_DONE;
		decide_and_throttle(LVSYS_INTR, lbat_lv, event);
	} else {
		if (event == lbat_data->lvsys_thd_volt_l)
			decide_and_throttle(LVSYS_INTR, 1, lbat_data->lvsys_thd_volt_l);
		else if (event == lbat_data->lvsys_thd_volt_h)
			decide_and_throttle(LVSYS_INTR, 0, lbat_data->lvsys_thd_volt_h);
		else
			pr_notice("[%s] wrong lvsys thd = %lu\n", __func__, event);
	}
	return NOTIFY_DONE;
}

static struct notifier_block lvsys_notifier = {
	.notifier_call = lvsys_notifier_call,
};

static int fill_thd_info(struct platform_device *pdev, struct lbat_thl_priv *priv,
			u32 *volt_thd, int intr_no, enum LOW_BATTERY_USER_TAG user)
{
	int i, j, max_thr_lv, volt_t_size, ret;
	unsigned int volt_l[LOW_BATTERY_LEVEL_NUM-1], volt_h[LOW_BATTERY_LEVEL_NUM-1];
	struct lbat_thd_tbl *thd_info;
	int last_volt_t_size = 0;

	if (intr_no < 0 || intr_no >= INTR_MAX_NUM) {
		pr_notice("%s: invalid intr_no %d\n", __func__, intr_no);
		return -EINVAL;
	}
	if (user == LBAT_INTR_1 || user == LBAT_INTR_2) {
		max_thr_lv = LBAT_PMIC_MAX_LEVEL;

	} else if (user == LVSYS_INTR) {
		max_thr_lv = priv->lvsys_volt_size;
	} else {
		pr_notice("%s: invalid user\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i <= priv->temp_max_stage; i++) {
		for (j = 0; j < max_thr_lv; j++) {
			volt_l[j] = volt_thd[i * max_thr_lv + j];
			volt_h[j] = volt_thd[max_thr_lv * (priv->temp_max_stage + 1) + i * max_thr_lv + j];
		}

		ret = check_duplicate(volt_l, user, priv);
		ret |= check_duplicate(volt_h, user, priv);
		if (ret < 0) {
			pr_notice("%s: check duplicate error\n", __func__);
			return -EINVAL;
		}

		thd_info = &priv->lbat_thd[intr_no][i];
		if (pdev!=NULL){
			thd_info->thd_volts_size = max_thr_lv * 2;
			thd_info->lbat_intr_info = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
				sizeof(struct lbat_thd_tbl), GFP_KERNEL);
			if (!thd_info->lbat_intr_info)
				return -ENOMEM;
		}

		volt_t_size = rearrange_volt(thd_info->lbat_intr_info, &volt_l[0], &volt_h[0], max_thr_lv);
		if (volt_t_size <= 0) {
			pr_notice("[%s] Failed to rearrange_volt\n", __func__);
			return -ENODATA;
		}

		// different temp stage volt size should be the same
		if (i != 0 && last_volt_t_size != volt_t_size) {
			pr_notice("[%s] volt size should be the same, force trigger kernel panic\n", __func__);
			BUG_ON(1);
		}
		last_volt_t_size = volt_t_size;

		if(pdev!=NULL){
			thd_info->thd_volts_size = volt_t_size;
			thd_info->thd_volts = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
				sizeof(u32), GFP_KERNEL);
			if (!thd_info->thd_volts)
				return -ENOMEM;

			thd_info->ag_thd_volts = devm_kmalloc_array(&pdev->dev, thd_info->thd_volts_size,
				sizeof(u32), GFP_KERNEL);
			if (!thd_info->ag_thd_volts)
				return -ENOMEM;
		}
		mutex_lock(&exe_thr_lock);
		for (j = 0; j < volt_t_size; j++) {
			thd_info->thd_volts[j] = thd_info->lbat_intr_info[j].volt;
			thd_info->ag_thd_volts[j] = thd_info->lbat_intr_info[j].volt;
		}
		mutex_unlock(&exe_thr_lock);

		dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);
	}
	return 0;
}

static int low_battery_thd_setting(struct platform_device *pdev, struct lbat_thl_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	char thd_volts_l[THD_VOLTS_LENGTH], thd_volts_h[THD_VOLTS_LENGTH], str[THD_VOLTS_LENGTH];
	char throttle_lv[THD_VOLTS_LENGTH];
	int ret = 0, intr_num = 0;
	int num, i, volt_size, thl_level_size;
	u32 *volt_thd;
	struct device_node *gauge_np = pdev->dev.parent->of_node;

	if (vbat_thd_enable == 0 && lvsys_thd_enable == 0) {
		pr_notice("[%s] disable vbat and lvsys\n", __func__);
		return -EINVAL;
	}
	num = of_property_count_u32_elems(np, "temperature-stage-threshold");
	if (num < 0)
		num = 0;

	priv->temp_max_stage = num;

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
		ret = of_property_read_u32(gauge_np, "bat_type", &priv->bat_type);
		if (ret || priv->bat_type >= 10) {
			priv->bat_type = 0;
			dev_notice(&pdev->dev, "[%s] get bat_type fail, ret=%d bat_type=%d\n", __func__, ret,
				priv->bat_type);
		}
	}

	ret = snprintf(str, THD_VOLTS_LENGTH, "bat%d-aging-threshold", priv->bat_type);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	num = of_property_count_u32_elems(np, str);
	if (num < 0 || num > 10) {
		pr_info("error aging-threshold num %d, set to 0\n", num);
		num = 0;
	}
	priv->aging_max_stage = num;

	if (priv->aging_max_stage > 0) {
		priv->aging_thd = devm_kmalloc_array(&pdev->dev, priv->aging_max_stage, sizeof(u32), GFP_KERNEL);
		if (!priv->aging_thd)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, str, priv->aging_thd, num);
		if (ret) {
			pr_notice("get %s error %d, set aging_max_stage=0\n", str, ret);
			priv->aging_max_stage = 0;
		}

		priv->aging_volts = devm_kmalloc_array(&pdev->dev, priv->aging_max_stage * (priv->temp_max_stage + 1),
			sizeof(u32), GFP_KERNEL);
		if (!priv->aging_volts)
			return -ENOMEM;

		for (i = 0; i <= priv->temp_max_stage; i++) {
			ret = snprintf(str, THD_VOLTS_LENGTH, "bat%d-aging-volts-t%d", priv->bat_type, i);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			ret = of_property_read_u32_array(np, str, &priv->aging_volts[priv->aging_max_stage*i], num);
			if (ret) {
				pr_notice("get %s error %d, set aging_max_stage=0\n", str, ret);
				priv->aging_max_stage = 0;
				break;
			}
		}
	}

	thl_level_size = pmic_level_num * (priv->temp_max_stage + 1);
	priv->thl_lv = devm_kmalloc_array(&pdev->dev, thl_level_size, sizeof(u32), GFP_KERNEL);
	if (!priv->thl_lv)
		return -ENOMEM;

	ret = snprintf(throttle_lv, THD_VOLTS_LENGTH, "throttle-level");
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	if (of_property_read_u32_array(np, throttle_lv, priv->thl_lv, thl_level_size)) {
		pr_info("%s:%d: read throttle level error\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (vbat_thd_enable == 1) {
		volt_size = LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1);
		volt_thd = devm_kmalloc_array(&pdev->dev, volt_size * 2, sizeof(u32), GFP_KERNEL);
		if (!volt_thd)
			return -ENOMEM;

		for (i = 1; i <= INTR_MAX_NUM; i++) {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type,
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
				ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type, i + 1,
					VOLT_L_STR);
				if (ret < 0)
					pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
				ret = snprintf(thd_volts_h, THD_VOLTS_LENGTH, "bat%d-intr%d-%s", priv->bat_type,
					i + 1, VOLT_H_STR);
				if (ret < 0)
					pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

				if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
					|| of_property_read_u32_array(np, thd_volts_h,
					&volt_thd[volt_size], volt_size)) {
					pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
						thd_volts_l, thd_volts_h);
					return -EINVAL;
				}

				ret = fill_thd_info(pdev, priv, volt_thd, i, 0);
				if (ret) {
					pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
					return ret;
				}
				priv->lbat_intr_num++;
			}
		} else {
			ret = snprintf(thd_volts_l, THD_VOLTS_LENGTH, "bat%d-%s", priv->bat_type, VOLT_L_STR);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			ret = snprintf(thd_volts_h, THD_VOLTS_LENGTH, "bat%d-%s", priv->bat_type, VOLT_H_STR);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			if (of_property_read_u32_array(np, thd_volts_l, &volt_thd[0], volt_size)
				|| of_property_read_u32_array(np, thd_volts_h,  &volt_thd[volt_size],
				volt_size)) {
				pr_info("%s:%d: read volts error %s %s\n", __func__, __LINE__,
					thd_volts_l, thd_volts_h);
				return -EINVAL;
			}

			ret = fill_thd_info(pdev, priv, volt_thd, INTR_1, LBAT_INTR_1);
			if (ret) {
				pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
				return ret;
			} else
				priv->lbat_intr_num++;
		}
	}
	return 0;
}

static int lvsys_thd_setting(struct platform_device *pdev, struct lbat_thl_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int volt_size, k, num, i;
	char prop_name[64], str[THD_VOLTS_LENGTH];
	u32 *volt_thd;

	if(priv->lvsys_volt_size > 1){
		volt_size = priv->lvsys_volt_size * (priv->temp_max_stage + 1);
		if (volt_size < 0 || volt_size != of_property_count_u32_elems(np, "lvsys-thd-volt-h"))
			volt_size = 0;
		volt_thd = devm_kmalloc_array(&pdev->dev, volt_size * 2, sizeof(u32), GFP_KERNEL);
		if (!volt_thd)
			return -ENOMEM;
		if (of_property_read_u32_array(np, "lvsys-thd-volt-l", &volt_thd[0], volt_size)
			|| of_property_read_u32_array(np, "lvsys-thd-volt-h",  &volt_thd[volt_size],
			volt_size))
			return -EINVAL;
		lvsys_table_data = kzalloc(sizeof(struct multi_voltage_table), GFP_KERNEL);
		if (!lvsys_table_data)
			return -ENOMEM;

		lvsys_table_data->selected_table = 0;
		memcpy(lvsys_table_data->tables[0].low, volt_thd, volt_size * sizeof(u32));
		memcpy(lvsys_table_data->tables[0].high, volt_thd + volt_size, volt_size * sizeof(u32));

		if (switch_pt) {
			for (k = 1; k < max_tb_num; k++) {
				// Read low voltage array
				ret = snprintf(prop_name, sizeof(prop_name), "lvsys-thd-volt-tb%d-l", k);
				if (ret < 0)
					pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
				ret = of_property_read_u32_array(np, prop_name,
								lvsys_table_data->tables[k].low,
								volt_size);
				if (ret) {
					pr_notice("Failed to read thd-volts-tb%d-l. Error: %d\n", k, ret);
					switch_pt = false;
					return -ENOMEM;
				}
				// Read high voltage array
				ret = snprintf(prop_name, sizeof(prop_name), "lvsys-thd-volt-tb%d-h", k);
				if (ret < 0)
					pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
				ret = of_property_read_u32_array(np, prop_name,
								lvsys_table_data->tables[k].high,
								volt_size);
				if (ret) {
					pr_notice("Failed to read thd-volts-tb%d-h. Error: %d\n", k, ret);
					switch_pt = false;
					return -ENOMEM;
				}
			}
		}

		ret = fill_thd_info(pdev, priv, volt_thd, LVSYS_INT, LVSYS_INTR);
		if (ret) {
			pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
			return ret;
		}
	} else {
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
	}

	ret = snprintf(str, THD_VOLTS_LENGTH, "lvsys-aging-threshold");
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	num = of_property_count_u32_elems(np, str);
	if (num < 0 || num > 10) {
		pr_info("error lvsys-aging-threshold num %d, set to 0\n", num);
		num = 0;
	}
	priv->lvsys_aging_max_stage = num;

	if (priv->lvsys_aging_max_stage > 0) {
		priv->lvsys_aging_thd = devm_kmalloc_array(&pdev->dev,
						priv->lvsys_aging_max_stage, sizeof(u32), GFP_KERNEL);
		if (!priv->lvsys_aging_thd)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, str, priv->lvsys_aging_thd, num);
		if (ret) {
			pr_notice("get %s error %d, set lvsys_aging_max_stage=0\n", str, ret);
			priv->lvsys_aging_max_stage = 0;
		}

		priv->lvsys_aging_volts = devm_kmalloc_array(&pdev->dev,
						priv->lvsys_aging_max_stage * (priv->temp_max_stage + 1),
			sizeof(u32), GFP_KERNEL);
		if (!priv->lvsys_aging_volts)
			return -ENOMEM;

		for (i = 0; i <= priv->temp_max_stage; i++) {
			ret = snprintf(str, THD_VOLTS_LENGTH, "lvsys-aging-volts-t%d", i);
			if (ret < 0)
				pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

			ret = of_property_read_u32_array(np, str,
					&priv->lvsys_aging_volts[priv->lvsys_aging_max_stage*i], num);
			if (ret) {
				pr_notice("get %s error %d, set aging_max_stage=0\n", str, ret);
				priv->lvsys_aging_max_stage = 0;
				break;
			}
		}
	}
	return 0;
}

static int low_battery_thd_tbl_init(struct platform_device *pdev, struct lbat_thl_priv *priv)
{
	int ret, i;
	struct device_node *np = pdev->dev.of_node;
	char prop_name[64];

	if (!np) {
		switch_pt = false;
		return -EINVAL;
	}

	lbat_table_data = devm_kzalloc(&pdev->dev, sizeof(struct multi_voltage_table), GFP_KERNEL);
	if (!lbat_table_data){
		switch_pt = false;
		return -ENOMEM;
	}
	lbat_table_data->selected_table = 0;

	for (i = 0; i < max_tb_num; i++) {
		int volt_size = LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1);

		// Read low voltage array
		snprintf(prop_name, sizeof(prop_name), "bat%d-thd-volts-tb%d-l", priv->bat_type, i);
		ret = of_property_read_u32_array(np, prop_name,
						lbat_table_data->tables[i].low,
						volt_size);
		if (ret) {
			pr_notice("Failed to read thd-volts-tb%d-l. Error: %d\n", i, ret);
			switch_pt = false;
			devm_kfree(&pdev->dev, lbat_table_data);
			return -ENOMEM;
		}
		// Read high voltage array
		snprintf(prop_name, sizeof(prop_name), "bat%d-thd-volts-tb%d-h", priv->bat_type, i);
		ret = of_property_read_u32_array(np, prop_name,
						lbat_table_data->tables[i].high,
						volt_size);
		if (ret) {
			pr_notice("Failed to read thd-volts-tb%d-h. Error: %d\n", i, ret);
			switch_pt = false;
			devm_kfree(&pdev->dev, lbat_table_data);
			return -ENOMEM;
		}
	}
	switch_pt = true;
	return 0;
}

static void switch_voltage_table(unsigned int table, enum LOW_BATTERY_USER_TAG user)
{
	struct lbat_thl_priv *priv = lbat_data;
	u32 *volt_thd, *lvsys_volt_thd;
	int volt_size, ret, i, vbat_aging_stage, vsys_aging_stage;
	struct lbat_thd_tbl *thd_info;

	if (user == LBAT_INTR_1){
		volt_size = 2 * LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1);
		volt_thd = kmalloc_array(volt_size, sizeof(u32), GFP_KERNEL);

		if (!volt_thd)
			return;

		// Copy low voltage thresholds
		memcpy(volt_thd,
			lbat_table_data->tables[table].low,
			LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1) * sizeof(u32));

		// Copy high voltage thresholds
		memcpy(volt_thd + LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1),
			lbat_table_data->tables[table].high,
			LBAT_PMIC_MAX_LEVEL * (priv->temp_max_stage + 1) * sizeof(u32));

#ifdef LBAT2_ENABLE
		ret = fill_thd_info(NULL, priv, volt_thd, INTR_2, LBAT_INTR_2);
		thd_info = &lbat_data->lbat_thd[INTR_2][lbat_data->temp_reg_stage];
		lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_2],
				thd_info->ag_thd_volts, thd_info->thd_volts_size);
#else
		ret = fill_thd_info(NULL, priv, volt_thd, INTR_1, LBAT_INTR_1);
		thd_info = &lbat_data->lbat_thd[INTR_1][lbat_data->temp_reg_stage];
		lbat_user_modify_thd_ext_locked(lbat_data->lbat_pt[INTR_1],
				thd_info->ag_thd_volts, thd_info->thd_volts_size);
#endif

		dump_thd_volts_ext(thd_info->ag_thd_volts, thd_info->thd_volts_size);
		lbat_data->lbat_thd_modify = 0;

		if (ret)
			pr_info("Failed to switch voltage table. Error: %d\n", ret);
		else
			pr_info("Switched to voltage table %d\n", table);

		kfree(volt_thd);
	} else if (user == LVSYS_INTR){
		volt_size = lbat_data->lvsys_volt_size * (priv->temp_max_stage + 1);
		volt_thd = kmalloc_array(volt_size * 2, sizeof(u32), GFP_KERNEL);
		if (!volt_thd)
			return;
		memcpy(volt_thd,
			lvsys_table_data->tables[table].low,
			lbat_data->lvsys_volt_size * (priv->temp_max_stage + 1) * sizeof(u32));
		memcpy(volt_thd + lbat_data->lvsys_volt_size * (priv->temp_max_stage + 1),
			lvsys_table_data->tables[table].high,
			lbat_data->lvsys_volt_size * (priv->temp_max_stage + 1) * sizeof(u32));
		ret = fill_thd_info(NULL, priv, volt_thd, LVSYS_INT, LVSYS_INTR);
		if (ret) {
			pr_info("%s:%d: fill_thd_info error %d\n", __func__, __LINE__, ret);
			return;
		}
		lvsys_volt_thd = kmalloc_array(lbat_data->lvsys_volt_size * 2, sizeof(u32), GFP_KERNEL);
		if (!lvsys_volt_thd)
			return;
		mutex_lock(&exe_thr_lock);
		for (i = 0; i < lbat_data->lvsys_volt_size; i++) {
			lvsys_volt_thd[i * 2] = lvsys_table_data->tables[table].high[lbat_data->temp_reg_stage *
											lbat_data->lvsys_volt_size + i];
			lvsys_volt_thd[i * 2 + 1] = lvsys_table_data->tables[table].low[lbat_data->temp_reg_stage *
											lbat_data->lvsys_volt_size + i];
		}
		mutex_unlock(&exe_thr_lock);
		lvsys_modify_thd(lvsys_volt_thd, lbat_data->lvsys_volt_size * 2);
		kfree(lvsys_volt_thd);
		dump_lvsys_thd();
	}
	vbat_aging_stage = get_aging_stage(INTR_1);
	vsys_aging_stage = get_aging_stage(LVSYS_INT);
	update_thresholds(lbat_data->temp_reg_stage, vbat_aging_stage, vsys_aging_stage);

}

/*****************************************************************************
 * switch low battery threshold table
 ******************************************************************************/
static ssize_t lvbat_table_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (vbat_thd_enable == 0) {
		dev_info(dev, "vbat throttle disable\n");
		return -EINVAL;
	}
	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", lbat_table_data->selected_table);
}

static ssize_t lvbat_table_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int lbat_table;
	char keyword[10];

	if (vbat_thd_enable == 0) {
		dev_info(dev, "vbat throttle disable\n");
		return -EINVAL;
	}
	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if (sscanf(buf, "%9s %d", keyword, &lbat_table) == 2) {
		if (strcmp(keyword, "table_idx") == 0) {
			if (lbat_table < max_tb_num) {
				lbat_table_data->selected_table = lbat_table;
				switch_voltage_table(lbat_table, LBAT_INTR_1);
				pr_info("LVBAT switch to table %u\n", lbat_table_data->selected_table);
			} else {
				pr_info("Invalid lbat table index.\n");
			}
		} else {
			pr_info("Invalid keyword.\n");
		}
	} else {
		pr_info("Invalid input format.\n");
	}
	return count;
}

static DEVICE_ATTR_RW(lvbat_table);

/*****************************************************************************
 * switch low battery threshold table
 ******************************************************************************/
static ssize_t lvsys_table_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (lvsys_thd_enable == 0) {
		dev_info(dev, "lvsys throttle disable\n");
		return -EINVAL;
	}
	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", lvsys_table_data->selected_table);
}

static ssize_t lvsys_table_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int lvsys_table;
	char keyword[10];

	if (lvsys_thd_enable == 0) {
		dev_info(dev, "lvsys throttle disable\n");
		return -EINVAL;
	}
	if (switch_pt == false) {
		dev_info(dev, "not support switch_pt\n");
		return -EINVAL;
	}
	if (sscanf(buf, "%9s %d", keyword, &lvsys_table) == 2) {
		if (strcmp(keyword, "table_idx") == 0) {
			if (lvsys_table < max_tb_num) {
				lvsys_table_data->selected_table = lvsys_table;
				switch_voltage_table(lvsys_table, LVSYS_INTR);
				pr_info("LVSYS switch to table %u\n", lvsys_table_data->selected_table);
			} else {
				pr_info("Invalid lvsys table index.\n");
			}
		} else {
			pr_info("Invalid keyword.\n");
		}
	} else {
		pr_info("Invalid input format.\n");
	}
	return count;
}

static DEVICE_ATTR_RW(lvsys_table);

static int low_battery_register_setting(struct platform_device *pdev,
		struct lbat_thl_priv *priv, enum LOW_BATTERY_INTR_TAG intr_type,
		unsigned int temp_stage)
{
	int ret;
	struct lbat_thd_tbl *thd_info;
	struct lbat_user *lbat_p = NULL;

	if (intr_type == INTR_1 && temp_stage <= priv->temp_max_stage) {
		thd_info = &priv->lbat_thd[INTR_1][temp_stage];
		priv->lbat_pt[INTR_1] = lbat_user_register_ext("power throttling", thd_info->ag_thd_volts,
		thd_info->thd_volts_size, exec_low_battery_callback);
		lbat_p = priv->lbat_pt[INTR_1];
	} else if (intr_type == INTR_2  && temp_stage <= priv->temp_max_stage) {
#ifdef LBAT2_ENABLE
		thd_info = &priv->lbat_thd[INTR_2][temp_stage];
		priv->lbat_pt[INTR_2] = dual_lbat_user_register_ext("power throttling",
			thd_info->ag_thd_volts, thd_info->thd_volts_size, exec_dual_low_battery_callback);
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
	struct lbat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;

	switch_pt = true;
	ret = of_property_read_u32(np, "lbat-max-tb-num", &max_tb_num);
	if (ret || max_tb_num < 0 || max_tb_num > MAX_TABLES) {
		pr_notice("%s: get max_tb_num %d fail set to default %d\n", __func__, max_tb_num, ret);
		max_tb_num = 1;
		switch_pt = false;
	}
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	INIT_WORK(&priv->psy_work, psy_handler);

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
	if (switch_pt) {
		VOLT_L_STR = "thd-volts-tb0-l";
		VOLT_H_STR = "thd-volts-tb0-h";
	} else {
		VOLT_L_STR = "thd-volts-l";
		VOLT_H_STR = "thd-volts-h";
	}
	ret = of_property_read_u32(np, "lvsys-thd-volt-l-size", &priv->lvsys_volt_size);
	if (ret) {
		pr_info("[%s] failed to read lvsys-thd-volt-l-size, ret=%d\n", __func__, ret);
		priv->lvsys_volt_size = 1;
	}
	ret = of_property_read_u32(np, "lvsys-thd-volt-h-size", &priv->lvsys_volt_size);
	if (ret) {
		pr_info("[%s] failed to read lvsys-thd-volt-h-size, ret=%d\n", __func__, ret);
		priv->lvsys_volt_size = 1;
	}
	pmic_level_num = MAX(vbat_thd_enable * LBAT_PMIC_LEVEL_NUM, lvsys_thd_enable * (priv->lvsys_volt_size+1));
	ret = low_battery_thd_setting(pdev, priv);
	if (ret) {
		pr_info("[%s] low_battery_thd_setting error, ret=%d\n", __func__, ret);
		return ret;
	}
	if (vbat_thd_enable) {
		if(switch_pt != false){
			ret = low_battery_thd_tbl_init(pdev, priv);
			if (ret) {
				pr_info("[%s] low_battery_thd_tbl_init error, ret=%d\n", __func__, ret);
				switch_pt = false;
			}
		}

		for (i = 0; i < priv->lbat_intr_num; i++) {
			ret = low_battery_register_setting(pdev, priv, i, 0);
			if (ret) {
				pr_info("[%s] low_battery_register failed, intr_no=%d ret=%d\n",
					__func__, i, ret);
				return ret;
			}
		}
	}

	if (priv->temp_max_stage > 0 || priv->aging_max_stage > 0) {
		lbat_nb.notifier_call = lbat_psy_event;
		ret = power_supply_reg_notifier(&lbat_nb);
		if (ret) {
			dev_notice(&pdev->dev, "[%s] power_supply_reg_notifier fail\n",
				__func__);
			return ret;
		}
	}

	if (lvsys_thd_enable) {
		ret = lvsys_thd_setting(pdev, priv);
		if (ret) {
			pr_info("[%s] lvsys_thd_setting error, ret=%d\n", __func__, ret);
			return ret;
		}
		ret = lvsys_register_notifier(&lvsys_notifier);
		if (ret)
			dev_notice(&pdev->dev, "lvsys_register_notifier error ret=%d\n", ret);
	}

	ret = of_property_read_u32(np, "pt-shutdown-enable", &priv->pt_shutdown_en);
	if (ret) {
		dev_notice(&pdev->dev, "[%s] failed to get pt-shutdown, set to 1\n", __func__);
		priv->pt_shutdown_en = 1;
	}

	if (of_property_read_bool(np, "lvsys-LV1-trigger")) {
		lvsys_lv1_trigger = true;
	} else {
		dev_notice(&pdev->dev, "[%s] set lvsys as last level\n", __func__);
		lvsys_lv1_trigger = false;
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
		&dev_attr_low_battery_throttle_cnt);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_lvbat_table);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_lvsys_table);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_lvsys_modify_threshold);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}

	priv->dev = &pdev->dev;
	lbat_data = priv;
	if (priv->pt_shutdown_en)
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
