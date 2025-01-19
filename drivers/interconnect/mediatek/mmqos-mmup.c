// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/io.h>
#include "mmqos-mmup.h"
#include "vcp_status.h"
#include "mtk-mm-monitor-controller.h"

static bool mmqos_mmup_cb_ready;
static bool mmqos_mmup_init_done;
static struct notifier_block mmup_ready_notifier;
static struct notifier_block mmmc_smmu_factor_notifier;
static struct notifier_block mmmc_threshold_us_notifier;
static int mmup_power;
static DEFINE_MUTEX(mmqos_mmup_pwr_mutex);
static DEFINE_MUTEX(mmqos_mmup_ipi_mutex);
static int mmup_mmqos_log;
static uint32_t mmup_ipi_ack_data;


static inline bool mmqos_mmup_is_init_done(void)
{
	return (mmqos_state != MMQOS_DISABLE) ? mmqos_mmup_init_done : false;
}

int mtk_mmqos_enable_mmup(const bool enable)
{
	int ret = 0;

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmqos_mmup_pwr_mutex);
	if (enable) {
		if (!mmup_power) {
			ret = vcp_register_feature_ex(MMQOS_MMUP_FEATURE_ID);
			if (ret)
				goto enable_mmup_end;
		}
		mmup_power += 1;
	} else {
		if (!mmup_power) {
			ret = -EINVAL;
			goto enable_mmup_end;
		}
		if (mmup_power == 1) {

			ret = vcp_deregister_feature_ex(MMQOS_MMUP_FEATURE_ID);
			if (ret)
				goto enable_mmup_end;
		}
		mmup_power -= 1;
	}

enable_mmup_end:
	if (ret)
		MMQOS_ERR("ret:%d enable:%d mmup_power:%d",
			ret, enable, mmup_power);
	if (log_level & log_vcp_pwr)
		MMQOS_DBG("ret:%d enable:%d mmup_power:%d",
			ret, enable, mmup_power);
	mutex_unlock(&mmqos_mmup_pwr_mutex);
	return ret;
}

int mmqos_mmup_ipi_send(const u8 func, u32 data)
{
	struct mmqos_mmup_ipi_data slot = { func, 0, 0, data };
	int ret = 0, retry = 0;
	struct mtk_ipi_device *mmup_ipi_dev;

	if (func >= FUNC_MMUP_NUM) {
		MMQOS_ERR("func:%hhu >= FUNC_MMUP_NUM:%u", func, FUNC_MMUP_NUM);
		return -EINVAL;
	}

	if (!mmqos_mmup_is_init_done()) {
		MMQOS_ERR("mmqos mmup is not init done");
		return -ENODEV;
	}

	while (!is_vcp_ready_ex(MMQOS_MMUP_FEATURE_ID) ||
		(!mmqos_mmup_cb_ready && func != FUNC_MMUP_SYNC_STATE)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			ret = -ETIMEDOUT;
			goto mmup_ipi_done;
		}
		mdelay(1);
	}

	mutex_lock(&mmqos_mmup_ipi_mutex);
	mmup_ipi_dev = vcp_get_ipidev(MMQOS_MMUP_FEATURE_ID);
	if (!mmup_ipi_dev) {
		MMQOS_ERR("mmup_ipi_dev is null");
		ret = -ENODEV;
		goto mmup_ipi_unlock;
	}

	mmup_ipi_ack_data = 0;
	ret = mtk_ipi_send_compl(mmup_ipi_dev, IPI_OUT_MMQOS_MMUP, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMQOS, IPI_TIMEOUT_MS);
	if (!ret && !mmup_ipi_ack_data)
		ret = -EFAULT;

mmup_ipi_unlock:
	mutex_unlock(&mmqos_mmup_ipi_mutex);

mmup_ipi_done:
	if (ret)
		MMQOS_ERR(
			"ret:%d ready:%d cb_ready:%d slot:%#llx mmup_power:%d",
			ret, is_vcp_ready_ex(MMQOS_MMUP_FEATURE_ID), mmqos_mmup_cb_ready,
			*(u64 *)&slot, mmup_power);

	return ret;
}

static int mmqos_mmup_notifier_callback(struct notifier_block *nb,
	unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		MMQOS_DBG("receive VCP_EVENT_READY");
		mmqos_mmup_ipi_send(FUNC_MMUP_SYNC_STATE, mmqos_state);
		mmqos_mmup_cb_ready = true;
		mmqos_mmup_ipi_send(FUNC_MMUP_SMMU_FACTOR, get_ostdbl_smmu_factor());
		mmqos_mmup_ipi_send(FUNC_MMUP_THRESHOLD_US, get_axi_mon_threshold_us());
		break;
	case VCP_EVENT_STOP:
	case VCP_EVENT_SUSPEND:
		mmqos_mmup_cb_ready = false;
		break;
	}
	return NOTIFY_DONE;
}

static int mmqos_mmup_ipi_handle(u32 func)
{
	mtk_mmqos_enable_mmup(true);
	switch (func) {
	case FUNC_MMUP_SYNC_STATE:
		mmqos_mmup_ipi_send(FUNC_MMUP_SYNC_STATE, mmqos_state);
		break;
	case FUNC_MMUP_SMMU_FACTOR:
		mmqos_mmup_ipi_send(FUNC_MMUP_SMMU_FACTOR, get_ostdbl_smmu_factor());
		break;
	case FUNC_MMUP_THRESHOLD_US:
		mmqos_mmup_ipi_send(FUNC_MMUP_THRESHOLD_US, get_axi_mon_threshold_us());
		break;
	default:
		MMQOS_ERR("wrong func:%u", func);
	}
	mtk_mmqos_enable_mmup(false);
	return NOTIFY_DONE;
}

static int mmmc_smmu_factor_notifier_callback(struct notifier_block *nb,
	unsigned long action, void *data)
{
	mmqos_mmup_ipi_handle(FUNC_MMUP_SMMU_FACTOR);
	return 0;
}

static int mmmc_threshold_us_notifier_callback(struct notifier_block *nb,
	unsigned long action, void *data)
{
	mmqos_mmup_ipi_handle(FUNC_MMUP_THRESHOLD_US);
	return 0;
}

int mmqos_mmup_init_thread(void *data)
{
	static struct mtk_ipi_device *mmup_ipi_dev;
	int ret, retry = 0;

	while (mtk_mmqos_enable_mmup(true)) {
		if (++retry > 100) {
			MMQOS_ERR("mmup is not power on yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	retry = 0;
	while (!is_vcp_ready_ex(MMQOS_MMUP_FEATURE_ID)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			MMQOS_ERR("VCP_A_ID not ready");
			return -ETIMEDOUT;
		}
		mdelay(1);
	}

	retry = 0;
	while (!(mmup_ipi_dev = vcp_get_ipidev(MMQOS_MMUP_FEATURE_ID))) {
		if (++retry > 100) {
			MMQOS_ERR("cannot get mmup ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmqos_mmup_init_done = true;

	ret = mtk_ipi_register(mmup_ipi_dev, IPI_OUT_MMQOS_MMUP,
		NULL, NULL, &mmup_ipi_ack_data);
	if (ret)
		MMQOS_ERR("mtk_ipi_register failed:%d", ret);
	mmup_ready_notifier.notifier_call = mmqos_mmup_notifier_callback;
	vcp_A_register_notify_ex(MMQOS_MMUP_FEATURE_ID, &mmup_ready_notifier);
	mtk_mmqos_enable_mmup(false);

	mmmc_smmu_factor_notifier.notifier_call = mmmc_smmu_factor_notifier_callback;
	mtk_mmmc_smmu_factor_register_notifier(&mmmc_smmu_factor_notifier);
	mmmc_threshold_us_notifier.notifier_call = mmmc_threshold_us_notifier_callback;
	mtk_mmmc_threshold_us_register_notifier(&mmmc_threshold_us_notifier);
	return 0;
}
EXPORT_SYMBOL_GPL(mmqos_mmup_init_thread);


static int mmqos_get_mmup_mmqos_log(char *buf, const struct kernel_param *kp)
{
	int len = 0;

	if (!mmqos_mmup_is_init_done())
		return 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "MMUP LOG:%#x", mmup_mmqos_log);
	return len;
}

static int mmqos_set_mmup_mmqos_log(const char *val, const struct kernel_param *kp)
{
	u32 log = 0;
	int ret;

	if (!mmqos_mmup_is_init_done())
		return 0;

	ret = kstrtou32(val, 0, &log);
	if (ret) {
		MMQOS_ERR("failed:%d log:%#x", ret, log);
		return ret;
	}

	mmup_mmqos_log = log;
	mtk_mmqos_enable_mmup(true);
	ret = mmqos_mmup_ipi_send(FUNC_MMUP_LOG, mmup_mmqos_log);
	mtk_mmqos_enable_mmup(false);
	return 0;
}

static const struct kernel_param_ops mmqos_set_mmup_mmqos_log_ops = {
	.get = mmqos_get_mmup_mmqos_log,
	.set = mmqos_set_mmup_mmqos_log,
};

module_param_cb(mmup_mmqos_log, &mmqos_set_mmup_mmqos_log_ops, NULL, 0644);
MODULE_PARM_DESC(mmup_mmqos_log, "mmqos mmup log");

MODULE_LICENSE("GPL");
