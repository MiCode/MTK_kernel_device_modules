// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include "mbraink_power.h"
#include <mbraink_modules_ops_def.h>

static struct mbraink_power_ops _mbraink_power_ops;

int mbraink_power_init(void)
{
	_mbraink_power_ops.getVotingInfo = NULL;
	_mbraink_power_ops.getPowerInfo = NULL;
	_mbraink_power_ops.getVcoreInfo = NULL;
	_mbraink_power_ops.getWakeupInfo = NULL;
	_mbraink_power_ops.getSpmInfo = NULL;
	_mbraink_power_ops.getSpmL1Info = NULL;
	_mbraink_power_ops.getSpmL2Info = NULL;
	_mbraink_power_ops.getScpInfo = NULL;
	_mbraink_power_ops.getScpTaskInfo = NULL;
	_mbraink_power_ops.getModemInfo = NULL;
	_mbraink_power_ops.getSpmiInfo = NULL;
	_mbraink_power_ops.getUvloInfo = NULL;
	_mbraink_power_ops.getPmicVoltageInfo = NULL;
	_mbraink_power_ops.suspendprepare = NULL;
	_mbraink_power_ops.postsuspend = NULL;
	_mbraink_power_ops.getMmdvfsInfo = NULL;
	_mbraink_power_ops.getMmdvfsUserInfo = NULL;
	_mbraink_power_ops.getPowerThrottleHwInfo = NULL;
	_mbraink_power_ops.getLpmStateInfo = NULL;
	_mbraink_power_ops.getSpmiGlitchInfo = NULL;
	_mbraink_power_ops.getDvfsrcInfo = NULL;
	_mbraink_power_ops.getMMBWInfo = NULL;
	_mbraink_power_ops.deviceSuspend = NULL;
	_mbraink_power_ops.deviceResume = NULL;
	_mbraink_power_ops.getPowerThrottleHwOcInfo = NULL;
	_mbraink_power_ops.getPowerSmapInfo = NULL;
	return 0;
}

int mbraink_power_deinit(void)
{
	_mbraink_power_ops.getVotingInfo = NULL;
	_mbraink_power_ops.getPowerInfo = NULL;
	_mbraink_power_ops.getVcoreInfo = NULL;
	_mbraink_power_ops.getWakeupInfo = NULL;
	_mbraink_power_ops.getSpmInfo = NULL;
	_mbraink_power_ops.getSpmL1Info = NULL;
	_mbraink_power_ops.getSpmL2Info = NULL;
	_mbraink_power_ops.getScpInfo = NULL;
	_mbraink_power_ops.getScpTaskInfo = NULL;
	_mbraink_power_ops.getModemInfo = NULL;
	_mbraink_power_ops.getSpmiInfo = NULL;
	_mbraink_power_ops.getUvloInfo = NULL;
	_mbraink_power_ops.getPmicVoltageInfo = NULL;
	_mbraink_power_ops.suspendprepare = NULL;
	_mbraink_power_ops.postsuspend = NULL;
	_mbraink_power_ops.getMmdvfsInfo = NULL;
	_mbraink_power_ops.getMmdvfsUserInfo = NULL;
	_mbraink_power_ops.getPowerThrottleHwInfo = NULL;
	_mbraink_power_ops.getLpmStateInfo = NULL;
	_mbraink_power_ops.getSpmiGlitchInfo = NULL;
	_mbraink_power_ops.getDvfsrcInfo = NULL;
	_mbraink_power_ops.getMMBWInfo = NULL;
	_mbraink_power_ops.deviceSuspend = NULL;
	_mbraink_power_ops.deviceResume = NULL;
	_mbraink_power_ops.getPowerThrottleHwOcInfo = NULL;
	_mbraink_power_ops.getPowerSmapInfo = NULL;
	return 0;
}

int register_mbraink_power_ops(struct mbraink_power_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_power_ops.getVotingInfo = ops->getVotingInfo;
	_mbraink_power_ops.getPowerInfo = ops->getPowerInfo;
	_mbraink_power_ops.getVcoreInfo = ops->getVcoreInfo;
	_mbraink_power_ops.getWakeupInfo = ops->getWakeupInfo;
	_mbraink_power_ops.getSpmInfo = ops->getSpmInfo;
	_mbraink_power_ops.getSpmL1Info = ops->getSpmL1Info;
	_mbraink_power_ops.getSpmL2Info = ops->getSpmL2Info;
	_mbraink_power_ops.getScpInfo = ops->getScpInfo;
	_mbraink_power_ops.getScpTaskInfo = ops->getScpTaskInfo;
	_mbraink_power_ops.getModemInfo = ops->getModemInfo;
	_mbraink_power_ops.getSpmiInfo = ops->getSpmiInfo;
	_mbraink_power_ops.getUvloInfo = ops->getUvloInfo;
	_mbraink_power_ops.getPmicVoltageInfo = ops->getPmicVoltageInfo;
	_mbraink_power_ops.suspendprepare = ops->suspendprepare;
	_mbraink_power_ops.postsuspend = ops->postsuspend;
	_mbraink_power_ops.getMmdvfsInfo = ops->getMmdvfsInfo;
	_mbraink_power_ops.getMmdvfsUserInfo = ops->getMmdvfsUserInfo;
	_mbraink_power_ops.getPowerThrottleHwInfo = ops->getPowerThrottleHwInfo;
	_mbraink_power_ops.getLpmStateInfo = ops->getLpmStateInfo;
	_mbraink_power_ops.getSpmiGlitchInfo = ops->getSpmiGlitchInfo;
	_mbraink_power_ops.getDvfsrcInfo = ops->getDvfsrcInfo;
	_mbraink_power_ops.getMMBWInfo = ops->getMMBWInfo;
	_mbraink_power_ops.deviceSuspend = ops->deviceSuspend;
	_mbraink_power_ops.deviceResume = ops->deviceResume;
	_mbraink_power_ops.getPowerThrottleHwOcInfo = ops->getPowerThrottleHwOcInfo;
	_mbraink_power_ops.getPowerSmapInfo = ops->getPowerSmapInfo;
	return 0;
}
EXPORT_SYMBOL(register_mbraink_power_ops);

int unregister_mbraink_power_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_power_ops.getVotingInfo = NULL;
	_mbraink_power_ops.getPowerInfo = NULL;
	_mbraink_power_ops.getVcoreInfo = NULL;
	_mbraink_power_ops.getWakeupInfo = NULL;
	_mbraink_power_ops.getSpmInfo = NULL;
	_mbraink_power_ops.getSpmL1Info = NULL;
	_mbraink_power_ops.getSpmL2Info = NULL;
	_mbraink_power_ops.getScpInfo = NULL;
	_mbraink_power_ops.getScpTaskInfo = NULL;
	_mbraink_power_ops.getModemInfo = NULL;
	_mbraink_power_ops.getSpmiInfo = NULL;
	_mbraink_power_ops.getUvloInfo = NULL;
	_mbraink_power_ops.getPmicVoltageInfo = NULL;
	_mbraink_power_ops.suspendprepare = NULL;
	_mbraink_power_ops.postsuspend = NULL;
	_mbraink_power_ops.getMmdvfsInfo = NULL;
	_mbraink_power_ops.getMmdvfsUserInfo = NULL;
	_mbraink_power_ops.getPowerThrottleHwInfo = NULL;
	_mbraink_power_ops.getLpmStateInfo = NULL;
	_mbraink_power_ops.getSpmiGlitchInfo = NULL;
	_mbraink_power_ops.getDvfsrcInfo = NULL;
	_mbraink_power_ops.getMMBWInfo = NULL;
	_mbraink_power_ops.deviceSuspend = NULL;
	_mbraink_power_ops.deviceResume = NULL;
	_mbraink_power_ops.getPowerThrottleHwOcInfo = NULL;
	_mbraink_power_ops.getPowerSmapInfo = NULL;
	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_power_ops);

void mbraink_power_get_voting_info(struct mbraink_voting_struct_data *mbraink_vcorefs_src)
{
	if (mbraink_vcorefs_src == NULL) {
		pr_info("%s: power voting info is null.\n", __func__);
		return;
	}

	if (_mbraink_power_ops.getVotingInfo)
		_mbraink_power_ops.getVotingInfo(mbraink_vcorefs_src);
	else
		pr_info("%s: Do not support ioctl get power voting info query.\n", __func__);

}


int mbraink_get_power_info(char *buffer, unsigned int size, int datatype)
{
	int ret = 0;

	if (buffer == NULL) {
		pr_info("%s: power info buffer is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getPowerInfo)
		ret = _mbraink_power_ops.getPowerInfo(buffer, size, datatype);
	else
		pr_info("%s: Do not support ioctl get power info query.\n", __func__);

	return ret;
}

int mbraink_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo)
{
	int ret = 0;

	if (pmbrainkPowerVcoreInfo == NULL) {
		pr_info("%s: power vcore info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getVcoreInfo)
		ret = _mbraink_power_ops.getVcoreInfo(pmbrainkPowerVcoreInfo);
	else
		pr_info("%s: Do not support ioctl get power vcore info query.\n", __func__);

	return ret;
}

void mbraink_get_power_wakeup_info(struct mbraink_power_wakeup_data *wakeup_info_buffer)
{
	if (wakeup_info_buffer == NULL) {
		pr_info("%s: power wakeup info is null.\n", __func__);
		return;
	}

	if (_mbraink_power_ops.getWakeupInfo)
		_mbraink_power_ops.getWakeupInfo(wakeup_info_buffer);
	else
		pr_info("%s: Do not support ioctl get power wakeup info query.\n", __func__);
}

int mbraink_power_get_spm_info(struct mbraink_power_spm_raw *power_spm_buffer)
{
	int ret = 0;

	if (power_spm_buffer == NULL) {
		pr_info("%s: power spm buffer is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getSpmInfo)
		ret = _mbraink_power_ops.getSpmInfo(power_spm_buffer);
	else
		pr_info("%s: Do not support ioctl get power spm info query.\n", __func__);

	return ret;
}

int mbraink_power_get_spm_l1_info(long long *spm_l1_array, int spm_l1_size)
{
	int ret = 0;

	if (spm_l1_array == NULL) {
		pr_info("%s: power spm l1 array is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getSpmL1Info)
		ret = _mbraink_power_ops.getSpmL1Info(spm_l1_array, spm_l1_size);
	else
		pr_info("%s: Do not support ioctl get power spm l1 info query.\n", __func__);

	return ret;
}

int mbraink_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	int ret = 0;

	if (spm_l2_info == NULL) {
		pr_info("%s: power spm l2 array is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getSpmL2Info)
		ret = _mbraink_power_ops.getSpmL2Info(spm_l2_info);
	else
		pr_info("%s: Do not support ioctl get power spm l2 info query.\n", __func__);

	return ret;
}


int mbraink_power_get_scp_info(struct mbraink_power_scp_info *scp_info)
{
	int ret = 0;

	if (scp_info == NULL) {
		pr_info("%s: power scp info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getScpInfo)
		ret = _mbraink_power_ops.getScpInfo(scp_info);
	else
		pr_info("%s: Do not support ioctl get power scp info query.\n", __func__);

	return ret;
}

int mbraink_power_get_scp_task_info(struct mbraink_power_scp_task_info *scp_info)
{
	int ret = 0;

	if (scp_info == NULL) {
		pr_info("%s: power scp task info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getScpTaskInfo)
		ret = _mbraink_power_ops.getScpTaskInfo(scp_info);
	else
		pr_info("%s: Do not support ioctl get power scp task info query.\n", __func__);

	return ret;
}


int mbraink_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	int ret = 0;

	if (modem_buffer == NULL) {
		pr_info("%s: power modem buffer is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getModemInfo)
		ret = _mbraink_power_ops.getModemInfo(modem_buffer);
	else
		pr_info("%s: Do not support ioctl get power modem info query.\n", __func__);

	return ret;
}

int mbraink_power_get_spmi_info(struct mbraink_spmi_struct_data *mbraink_spmi_data)
{
	int ret = 0;

	if (mbraink_spmi_data == NULL) {
		pr_info("%s: power spmi data is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getSpmiInfo)
		ret = _mbraink_power_ops.getSpmiInfo(mbraink_spmi_data);
	else
		pr_info("%s: Do not support ioctl get power spmi info query.\n", __func__);

	return ret;
}

int mbraink_power_get_uvlo_info(struct mbraink_uvlo_struct_data *mbraink_uvlo_data)
{
	int ret = 0;

	if (mbraink_uvlo_data == NULL) {
		pr_info("%s: power uvlo data is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getUvloInfo)
		ret = _mbraink_power_ops.getUvloInfo(mbraink_uvlo_data);
	else
		pr_info("%s: Do not support ioctl get power uvlo info query.\n", __func__);

	return ret;
}

int mbraink_power_get_pmic_voltage_info(struct mbraink_pmic_voltage_info *pmicVoltageInfo)
{
	int ret = 0;

	if (pmicVoltageInfo == NULL) {
		pr_info("%s: power pmic voltage is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getPmicVoltageInfo)
		ret = _mbraink_power_ops.getPmicVoltageInfo(pmicVoltageInfo);
	else
		pr_info("%s: Do not support ioctl get power pmic voltage info query.\n", __func__);

	return ret;
}

void mbraink_power_suspend_prepare(void)
{
	if (_mbraink_power_ops.suspendprepare)
		_mbraink_power_ops.suspendprepare();
}

void mbraink_power_post_suspend(long long last_resume_timestamp)
{
	if (_mbraink_power_ops.postsuspend)
		_mbraink_power_ops.postsuspend(last_resume_timestamp);
}

int mbraink_power_device_suspend(struct device *dev)
{
	int ret = 0;

	if (_mbraink_power_ops.deviceSuspend)
		ret = _mbraink_power_ops.deviceSuspend(dev);

	return ret;
}

int mbraink_power_device_resume(struct device *dev)
{
	int ret = 0;

	if (_mbraink_power_ops.deviceResume)
		ret = _mbraink_power_ops.deviceResume(dev);

	return ret;
}

int mbraink_power_get_mmdvfs_info(struct mbraink_mmdvfs_info *mmdvfsInfo)
{
	int ret = 0;

	if (mmdvfsInfo == NULL) {
		pr_info("%s: power mmdvfs is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getMmdvfsInfo)
		ret = _mbraink_power_ops.getMmdvfsInfo(mmdvfsInfo);
	else
		pr_info("%s: Do not support ioctl get power mmdvfs info query.\n", __func__);

	return ret;
}

int mbraink_power_get_mmdvfs_user_info(struct mbraink_mmdvfs_user_info *mmdvfs_user_info)
{
	int ret = 0;

	if (mmdvfs_user_info == NULL) {
		pr_info("%s: power mmdvfs user is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getMmdvfsUserInfo)
		ret = _mbraink_power_ops.getMmdvfsUserInfo(mmdvfs_user_info);
	else
		pr_info("%s: Do not support ioctl get power mmdvfs user info query.\n", __func__);

	return ret;
}

int mbraink_power_get_power_throttle_hw_info(struct mbraink_power_throttle_hw_data *power_throttle_hw_data)
{
	int ret = 0;

	if (power_throttle_hw_data == NULL) {
		pr_info("%s: power throttle hw info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getPowerThrottleHwInfo)
		ret = _mbraink_power_ops.getPowerThrottleHwInfo(power_throttle_hw_data);
	else
		pr_info("%s: Do not support ioctl get power throttle hw info query.\n", __func__);

	return ret;
}

int mbraink_power_get_lpmstate_info(struct mbraink_lpm_state_data *lpmStateInfo)
{
	int ret = 0;

	if (lpmStateInfo == NULL) {
		pr_info("%s: power lpm state is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getLpmStateInfo)
		ret = _mbraink_power_ops.getLpmStateInfo(lpmStateInfo);
	else
		pr_info("%s: Do not support ioctl get power lpm state info query.\n", __func__);

	return ret;
}

int mbraink_power_get_spmi_glitch_info(
	struct mbraink_spmi_glitch_struct_data *mbraink_spmi_glith_data)
{
	int ret = 0;

	if (mbraink_spmi_glith_data == NULL) {
		pr_info("%s: power spmi glitch data is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getSpmiGlitchInfo)
		ret = _mbraink_power_ops.getSpmiGlitchInfo(mbraink_spmi_glith_data);
	else
		pr_info("%s: Do not support ioctl get power spmi glitch info query.\n", __func__);

	return ret;
}

int mbraink_power_get_dvfsrc_info(
	struct mbraink_dvfsrc_struct_data *mbraink_dvfsrc_data)
{
	int ret = 0;

	if (mbraink_dvfsrc_data == NULL) {
		pr_info("%s: power dvfsrc data is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getDvfsrcInfo)
		ret = _mbraink_power_ops.getDvfsrcInfo(mbraink_dvfsrc_data);
	else
		pr_info("%s: Do not support ioctl get power dvfsrc info query.\n", __func__);

	return ret;
}

int mbraink_power_get_mmqos_bw_info(struct mbraink_mmqos_bw_info *mmqos_bw_info)
{
	int ret = 0;

	if (mmqos_bw_info == NULL) {
		pr_info("%s: power mm qos bw is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getMMBWInfo)
		ret = _mbraink_power_ops.getMMBWInfo(mmqos_bw_info);
	else
		pr_info("%s: Do not support ioctl get power mm qos bw info query.\n", __func__);

	return ret;
}

int mbraink_power_get_power_throttle_hw_oc_info(
	struct mbraink_power_throttle_hw_oc_data *pt_hw_oc_data)
{
	int ret = 0;

	if (pt_hw_oc_data == NULL) {
		pr_info("%s: power throttle hw oc info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getPowerThrottleHwOcInfo)
		ret = _mbraink_power_ops.getPowerThrottleHwOcInfo(pt_hw_oc_data);
	else
		pr_info("%s: Do not support ioctl get pt hw oc info query.\n", __func__);

	return ret;
}

int mbraink_power_get_power_smap_info(
	struct mbraink_power_smap_info *mbraink_smap_data)
{
	int ret = 0;

	if (mbraink_smap_data == NULL) {
		pr_info("%s: power smap info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_power_ops.getPowerSmapInfo)
		ret = _mbraink_power_ops.getPowerSmapInfo(mbraink_smap_data);
	else
		pr_info("%s: Do not support ioctl get smap info query.\n", __func__);

	return ret;
}

