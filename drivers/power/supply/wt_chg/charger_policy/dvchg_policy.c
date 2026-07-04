// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>

#include "../charger_class/adapter_class.h"
#include "../charger_class/charger_class.h"
#include "../charger_class/dvchg_class.h"
#include "dvchg_policy.h"

static const char *sm_str[] = {
	"PM_STATE_CHECK_DEV",
	"PM_STATE_INIT",
	"PM_STATE_MEASURE_RCABLE",
	"PM_STATE_ENABLE_DVCHG",
	"PM_STATE_DVCHG_CC_CV",
	"PM_STATE_DVCHG_EXIT",
};

#define DVCHG_POLICY_LOOP_TIME          300         // ms
#define DVCHG_POLICY_EXIT_THREAD_TIME   -1

#define DVCHG_POLICY_REQUEST_SAFE5V     5000
#define DVCHG_POLICY_FIRST_REQUEST_CURR 1000

#define DVCHG_POLICY_RECOVER_CNT        10

struct dvchg_policy *g_policy;
EXPORT_SYMBOL_GPL(g_policy);

static const struct dvchg_policy_config cfg = {
	.cv = 4480,
	.cc = 5000,
	.exit_cc = 2000,
	.min_vbat = 3500,
	.step_volt = 20,

	.max_request_volt = 10000,
	.min_request_volt = 6000,
	.max_request_curr = 3000,

	.max_ibat = 5000,
	.max_vbat = 4480,

	.tbat_level_def			 = {0, 0, 5, 25, 70, 90, 100},
	.tbat_curlmt				= {0, 0, 300, 0, 600, 900,0},
	.tbat_recovery			  = 2,

};

static int dvchg_policy_set_state(struct dvchg_policy *policy,
								enum policy_state state)
{
	mutex_lock(&policy->state_lock);
	policy->state = state;
	mutex_unlock(&policy->state_lock);
	return 0;
}

static int dvchg_policy_wake_thread(struct dvchg_policy *policy)
{
	policy->run_thread = true;
	wake_up(&policy->wait_queue);
	return 0;
}

static void dvchg_policy_timer_func(struct timer_list *timer)
{
	struct dvchg_policy *policy = container_of(timer,
							struct dvchg_policy, policy_timer);
	dvchg_policy_wake_thread(policy);
}

static int dvchg_policy_start_timer(struct dvchg_policy *policy, uint32_t ms)
{
	del_timer(&policy->policy_timer);
	if (ms < 0)
		return 0;
	policy->policy_timer.expires = jiffies + msecs_to_jiffies(ms);
	policy->policy_timer.function = dvchg_policy_timer_func;
	add_timer(&policy->policy_timer);
	return 0;
}

static int dvchg_policy_end_timer(struct dvchg_policy *policy)
{
	del_timer(&policy->policy_timer);
	return 0;
}

static int dvchg_policy_update_tbat_level(struct dvchg_policy *policy)
{
	struct dvchg_policy_config *cfg = &policy->cfg;
	struct chg_info *info = &policy->info;
	switch (policy->tdie_lv) {
	case THERMAL_COLD:
		if (info->tdie >= cfg->tbat_level_def[THERMAL_COLD] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_VERY_COOL;
		}
		break;
	case THERMAL_VERY_COOL:
		if (info->tdie >= cfg->tbat_level_def[THERMAL_VERY_COOL] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_COOL;
		} else if (info->tdie <= cfg->tbat_level_def[THERMAL_COLD]) {
			policy->tdie_lv = THERMAL_COLD;
		}
		break;
	case THERMAL_COOL:
		if (info->tdie >= cfg->tbat_level_def[THERMAL_COOL] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_NORMAL;
		} else if (info->tdie <= cfg->tbat_level_def[THERMAL_VERY_COOL]) {
			policy->tdie_lv = THERMAL_VERY_COOL;
		}
		break;
	case THERMAL_NORMAL:
		if (info->tdie >= cfg->tbat_level_def[THERMAL_WARM] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_WARM;
		} else if (info->tdie <= cfg->tbat_level_def[THERMAL_COOL]){
			policy->tdie_lv = THERMAL_COOL;
		}
		break;
	case THERMAL_WARM:
		if (info->tdie >= cfg->tbat_level_def[THERMAL_VERY_WARM] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_VERY_WARM;
		} else if (info->tdie <= cfg->tbat_level_def[THERMAL_WARM]) {
			policy->tdie_lv = THERMAL_NORMAL;
		}
		break;
	case THERMAL_VERY_WARM:
		pr_info("%s , %d\n",__func__, cfg->tbat_level_def[THERMAL_VERY_WARM]);
		if (info->tdie >= cfg->tbat_level_def[THERMAL_HOT] + cfg->tbat_recovery) {
			policy->tdie_lv = THERMAL_HOT;
		} else if (info->tdie <= cfg->tbat_level_def[THERMAL_VERY_WARM]) {
			policy->tdie_lv = THERMAL_WARM;
		}
		break;
	case THERMAL_HOT:
		if (info->tdie <= cfg->tbat_level_def[THERMAL_HOT]) {
			policy->tdie_lv = THERMAL_VERY_WARM;
		}
		break;
	default:
		break;
	}
	return 0;
}


static int dvchg_policy_update_chg_info(struct dvchg_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct charger_dev *charger = policy->use_charger;
	struct dvchg_dev *charger_pump = policy->use_dvchg;
	struct adapter_dev *adapter = policy->use_adapter;

	uint32_t adc[SC_ADC_MAX] = {0};
	uint8_t i = 0;
	int ret = 0;

	for (i = 0; i < SC_ADC_MAX; i++) {
		ret = dvchg_get_adc_value(charger_pump, i, &adc[i]);
		if (ret < 0) {
			dev_info(policy->dev, "get dv adc failed\n");
			ret = charger_get_adc(charger, i, &adc[i]);
			if (ret < 0) {
				dev_info(policy->dev, "get chg adc failed\n");
				continue;
			}
		}
	}

	info->ibat = adc[SC_ADC_IBAT];
	ret = charger_get_adc(charger, SC_ADC_VBAT, &(info->vbat));
	//info->vbat = adc[SC_ADC_VBAT];
	info->vbus = adc[SC_ADC_VBUS];
	info->ibus = adc[SC_ADC_IBUS];
	info->tdie = adc[SC_ADC_TDIE];

	dvchg_get_is_enable(charger_pump, &info->dvchg_chging);
	adapter_get_temp(adapter, &info->tadapter);

	dvchg_policy_update_tbat_level(policy);

	return 0;
}

static int dvchg_policy_state_init(struct dvchg_policy *policy)
{
	struct chg_info *info = &policy->info;
	struct dvchg_policy_config *cfg = &policy->cfg;

	if (info->vbat < cfg->min_vbat) {
		dev_notice(policy->dev, "battery voltage too low\n");
		policy->next_time = 1000;
	} else if (info->vbat > cfg->cv - 50) {
		dev_notice(policy->dev, "battery voltage too high\n");
		policy->next_time = DVCHG_POLICY_EXIT_THREAD_TIME;
		dvchg_policy_set_state(policy, POLICY_STOP);
	} else {
		policy->sm = PM_STATE_MEASURE_RCABLE;
		policy->next_time = 50;
	}

	return 0;
}

static int dvchg_policy_state_measure_rcable(struct dvchg_policy *policy)
{
	charger_set_input_curr_lmt(policy->use_charger, 0);
	policy->sm = PM_STATE_ENABLE_DVCHG;
	policy->next_time = 50;
	return 0;
}

static int dvchg_policy_state_enable_dvchg(struct dvchg_policy *policy)
{
	struct chg_info *info = &policy->info;
	static int cnt = 0;
#define DVCHG_POLICY_ENABLE_RETRY_CNT   30
	uint32_t status = 0;
	if (info->dvchg_chging) {
		policy->sm = PM_STATE_DVCHG_CC_CV;
		policy->next_time = 50;
		cnt = 0;
		goto out;
	} else {
		dvchg_set_enable(policy->use_dvchg, true);
	}

	dvchg_get_status(policy->use_dvchg, &status);

	if (policy->request_volt <= DVCHG_POLICY_REQUEST_SAFE5V) {
		policy->request_volt = info->vbat * 220 / 100;
	} else if(status & DVCHG_ERROR_VBUS_HIGH) {
		policy->request_volt -= 100;
	} else {
		policy->request_volt += 100;
	}
	policy->request_curr = DVCHG_POLICY_FIRST_REQUEST_CURR;

	adapter_set_cap(policy->use_adapter, policy->cap_nr, policy->request_volt,
									policy->request_curr);

	policy->next_time = 100;

	if (cnt++ > DVCHG_POLICY_ENABLE_RETRY_CNT) {
		dev_info(policy->dev, "%s enable dvchg failed\n", __func__);
		cnt = 0;
		policy->next_time = DVCHG_POLICY_EXIT_THREAD_TIME;
		dvchg_policy_set_state(policy, POLICY_STOP);
	}
out:
	return 0;
}

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_DVCHG_DONE,
};

static int __dvchg_cc_cv_algo(struct dvchg_policy *policy)
{
	int step = 0, step_vbat = 0, step_ibus = 0;
	int ibus_lmt, vbat_lmt;
	struct chg_info *info = &policy->info;
	struct dvchg_policy_config *cfg = &policy->cfg;
#define DVCHG_POLICY_END_CHG_CNT	 30
	static int cnt = 0;

	// end dvchg judge
	if ((info->vbat > (cfg->cv - 50)) &&
					(info->ibus < cfg->exit_cc / 2)) {
		if (cnt++ > DVCHG_POLICY_END_CHG_CNT)
			return PM_ALGO_RET_DVCHG_DONE;
	} else {
		cnt = 0;
	}

	policy->fast_request = true;

	vbat_lmt = min(cfg->cv, cfg->max_vbat);
	ibus_lmt = min(cfg->cc / 2, cfg->max_request_curr);

	// battery voltage loop
	if (info->vbat > vbat_lmt)
		step_vbat = -cfg->step_volt;
	else if (info->vbat < vbat_lmt - 7)
		step_vbat = cfg->step_volt;

	if (abs(info->vbat - vbat_lmt) < 200)
		policy->fast_request = false;

	// ibus loop
	if (cfg->tbat_curlmt[policy->tdie_lv] >= 0)
		ibus_lmt -= cfg->tbat_curlmt[policy->tdie_lv];
	if (info->ibus < ibus_lmt - 50) {
		step_ibus = cfg->step_volt;
	} else if (info->ibus > ibus_lmt) {
		step_ibus = -cfg->step_volt;
	}

	if (abs(info->ibus - ibus_lmt) < 600)
		policy->fast_request = false;

	step = min(step_ibus, step_vbat);

	if (policy->fast_request)
		policy->request_volt += step * 3;
	else
		policy->request_volt += step;

	policy->request_curr = ibus_lmt;

	adapter_set_cap(policy->use_adapter, policy->cap_nr, policy->request_volt,
									policy->request_curr);
	return PM_ALGO_RET_OK;
}

static int dvchg_policy_state_dvchg_cc_cv(struct dvchg_policy *policy)
{
	struct chg_info *info = &policy->info;
	int ret;

	ret = __dvchg_cc_cv_algo(policy);

	if (!info->dvchg_chging) {
		policy->recover = true;
		policy->sm = PM_STATE_DVCHG_EXIT;
		policy->next_time = 50;
		return 0;
	}

	if (ret == PM_ALGO_RET_DVCHG_DONE) {
		policy->recover = false;
		policy->sm = PM_STATE_DVCHG_EXIT;
	}

	if (policy->fast_request)
		policy->next_time = 50;
	else
		policy->next_time = DVCHG_POLICY_LOOP_TIME;
	return 0;
}

static int dv_policy_state_dvchg_exit(struct dvchg_policy *policy)
{
	struct chg_info *info = &policy->info;

	if (policy->request_volt != DVCHG_POLICY_REQUEST_SAFE5V) {
		policy->request_volt = DVCHG_POLICY_REQUEST_SAFE5V;
	}

	adapter_set_cap(policy->use_adapter, policy->cap_nr, policy->request_volt,
									policy->cap.curr_max[policy->cap_nr]);

	if (info->dvchg_chging) {
		dvchg_set_enable(policy->use_dvchg, false);
		goto out;
	}

	if (policy->recover && policy->recover_cnt++ < DVCHG_POLICY_RECOVER_CNT) {
		policy->recover = false;
		policy->sm = PM_STATE_INIT;
		goto out;
	}

	policy->next_time = DVCHG_POLICY_EXIT_THREAD_TIME;
	dvchg_policy_set_state(policy, POLICY_STOP);
	return 0;
out:
	policy->next_time = 50;
	return 0;
}

static int dv_policy_state_check_dev(struct dvchg_policy *policy);

static int dvchg_policy_thread_fn(void *data)
{
	struct dvchg_policy *policy = data;
	struct chg_info *info = &policy->info;
	int ret = 0;

	while (true) {
		ret = wait_event_interruptible(policy->wait_queue,
						   policy->run_thread);
		if (kthread_should_stop() || ret) {
			dev_notice(policy->dev, "%s exits(%d)\n", __func__, ret);
			break;
		}

		policy->run_thread = false;
		mutex_lock(&policy->running_lock);

		if (policy->state != POLICY_RUNNING)
			continue;

		dvchg_policy_update_chg_info(policy);

		dev_info(policy->dev, "state phase [%s]\n",sm_str[policy->sm]);
		dev_info(policy->dev, "ibat = %d, vbat = %d,vbus = %d,ibus = %d,tdie = %d,tadapter = %d, tdie_lv = %d\n",
				info->ibat, info->vbat, info->vbus, info->ibus, info->tdie, info->tadapter, policy->tdie_lv);

		switch (policy->sm) {
		case PM_STATE_CHECK_DEV:
			dv_policy_state_check_dev(policy);
			break;
		case PM_STATE_INIT:
			dvchg_policy_state_init(policy);
			break;
		case PM_STATE_MEASURE_RCABLE:
			dvchg_policy_state_measure_rcable(policy);
			break;
		case PM_STATE_ENABLE_DVCHG:
			dvchg_policy_state_enable_dvchg(policy);
			break;
		case PM_STATE_DVCHG_CC_CV:
			dvchg_policy_state_dvchg_cc_cv(policy);
			break;
		case PM_STATE_DVCHG_EXIT:
			dv_policy_state_dvchg_exit(policy);
			break;
		default:
			break;
		}

		dvchg_policy_start_timer(policy, policy->next_time);

		mutex_unlock(&policy->running_lock);
	}
	return 0;
}

static bool dvchg_policy_check_adapter_cap(struct dvchg_policy *policy,
										struct adapter_cap *cap)
{
	int i = 0;
	uint32_t curr = 0;
	struct dvchg_policy_config *cfg = &policy->cfg;
	for (i = 0; i < cap->cnt; i++) {
		pr_info("max volt : %d, min volt : %d, max curr : %d, min curr : %d\n",
				cap->volt_max[i], cap->volt_min[i], cap->curr_max[i], cap->curr_min[i]);
		if (cap->volt_max[i] >= cfg->max_request_volt &&
			cap->volt_min[i] <= cfg->min_request_volt &&
			cap->curr_max[i] >= cfg->max_request_curr) {
			if (cap->curr_max[i] < curr)
				continue;
			curr = cap->curr_max[i];
			policy->cap_nr = i;
		}
	}

	if (!curr)
		return false;
	return true;
}

static struct adapter_dev *dvchg_policy_check_adapter(struct dvchg_policy *policy)
{
	int i = 0, ret = 0;
	struct adapter_dev *adapter = NULL;
	for (i = 0; i < ARRAY_SIZE(policy->adapter_name); i++) {
		adapter = NULL;
		if (policy->adapter_name[i] == NULL)
			continue;
		adapter = adapter_find_dev_by_name(policy->adapter_name[i]);
		if (adapter == NULL)
			continue;
		// handshake
		ret = adapter_handshake(adapter);
		if (ret < 0) {
			pr_info("%s adapter handshake failed\n", policy->adapter_name[i]);
			goto out;
		}
		// check volt,curr
		ret = adapter_get_cap(adapter, &policy->cap);
		if (ret < 0) {
			pr_info("%s adapter get cap failed\n", policy->adapter_name[i]);
			goto out;
		}
		if (dvchg_policy_check_adapter_cap(policy, &policy->cap)) {
			break;
		}
out:
		adapter_reset(adapter);
	}
	return adapter;
}

static int dv_policy_state_check_dev(struct dvchg_policy *policy)
{
	struct adapter_dev *adapter;
	adapter = dvchg_policy_check_adapter(policy);
	if (adapter == NULL) {
		dvchg_policy_set_state(policy, POLICY_NO_SUPPORT);
		goto out;
	}
	dev_info(policy->dev, "adapter {%s} support charger pump policy\n",
							adapter->name);
	adapter_set_wdt(adapter, 0);
	policy->use_adapter = adapter;
	policy->use_charger = charger_find_dev_by_name("sc_charger");
	policy->use_dvchg = dvchg_find_dev_by_name("sc_dvchg");
	if (!policy->use_charger || !policy->use_dvchg) {
		dvchg_policy_set_state(policy, POLICY_NO_SUPPORT);
		goto out;
	}
	policy->sm = PM_STATE_INIT;
	policy->recover = false;
	policy->recover_cnt = 0;
	policy->next_time = 0;
	policy->request_volt = 0;
	policy->request_curr = 0;
	mutex_unlock(&policy->access_lock);
	return 0;

out:
	policy->next_time = -1;
	mutex_unlock(&policy->access_lock);
	return -EOPNOTSUPP;
}

int dvchg_policy_start(struct dvchg_policy *policy)
{
	pr_info("%s\n",__func__);
	if (mutex_trylock(&policy->access_lock) == 0)
		return -EBUSY;
	policy->sm = PM_STATE_CHECK_DEV;
	dvchg_policy_set_state(policy, POLICY_RUNNING);
	dvchg_policy_wake_thread(policy);
	return 0;
}
EXPORT_SYMBOL(dvchg_policy_start);

int dvchg_policy_stop(struct dvchg_policy *policy)
{
	mutex_lock(&policy->running_lock);
	mutex_unlock(&policy->access_lock);
	policy->request_volt = 0;
	dvchg_policy_end_timer(policy);
	adapter_reset(policy->use_adapter);
	dvchg_policy_set_state(policy, POLICY_NO_START);
	mutex_unlock(&policy->running_lock);
	return 0;
}
EXPORT_SYMBOL(dvchg_policy_stop);

static int dvchg_policy_probe(struct platform_device *pdev)
{
	struct dvchg_policy *policy;

	pr_info("%s start\n", __func__);

	policy = devm_kzalloc(&pdev->dev, sizeof(*policy), GFP_KERNEL);
	if (!policy)
		return -ENOMEM;

	g_policy = policy;

	policy->dev = &pdev->dev;
	platform_set_drvdata(pdev, policy);
	init_waitqueue_head(&policy->wait_queue);
	policy->thread = kthread_run(dvchg_policy_thread_fn, policy,
							"dvchg_policy_thread");
	
	//policy->adapter_name[0] = "vfcp adapter";
	//policy->adapter_name[1] = "ufcs_port2";
	policy->adapter_name[0] = "pd_adapter1";

	memcpy(&policy->cfg, &cfg, sizeof(cfg));

	mutex_init(&policy->state_lock);
	mutex_init(&policy->running_lock);
	mutex_init(&policy->access_lock);

	pr_info("%s success\n", __func__);

	return 0;
}

static int dvchg_policy_remove(struct platform_device *pdev)
{
	//struct dvchg_policy *policy = platform_get_drvdata(pdev);

	return 0;
}

static const struct of_device_id dvchg_policy_match[] = {
    {.compatible = "southchip,dvchg_policy",},
    {},
};
MODULE_DEVICE_TABLE(of, dvchg_policy_match);

static struct platform_driver dvchg_policy = {
    .probe = dvchg_policy_probe,
    .remove = dvchg_policy_remove,
    .driver = {
        .name = "dvchg_policy",
        .of_match_table = dvchg_policy_match,
    },
};

module_platform_driver(dvchg_policy);

MODULE_AUTHOR("Boqiang Liu <air-liu@southchip.com>");
MODULE_DESCRIPTION("Southchip Charger Pump Policy Core");
MODULE_LICENSE("GPL v2");
