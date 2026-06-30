// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "mbraink_memory.h"
#include <mbraink_modules_ops_def.h>

static struct mbraink_memory_ops _mbraink_memory_ops;

int mbraink_memory_init(void)
{
	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;
	_mbraink_memory_ops.getEmiInfo = NULL;
	_mbraink_memory_ops.getCmProfileInfo = NULL;
	_mbraink_memory_ops.getVsmrInfo = NULL;
	_mbraink_memory_ops.getCmVoteInfo = NULL;
	_mbraink_memory_ops.getCpuQosInfo = NULL;
	_mbraink_memory_ops.getMMQosInfo = NULL;
	_mbraink_memory_ops.getCmDDRVoteInfo = NULL;
	return 0;
}

int mbraink_memory_deinit(void)
{
	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;
	_mbraink_memory_ops.getEmiInfo = NULL;
	_mbraink_memory_ops.getCmProfileInfo = NULL;
	_mbraink_memory_ops.getVsmrInfo = NULL;
	_mbraink_memory_ops.getCmVoteInfo = NULL;
	_mbraink_memory_ops.getCpuQosInfo = NULL;
	_mbraink_memory_ops.getMMQosInfo = NULL;
	_mbraink_memory_ops.getCmDDRVoteInfo = NULL;
	return 0;
}

int register_mbraink_memory_ops(struct mbraink_memory_ops *ops)
{
	if (!ops)
		return -1;

	pr_info("%s: register.\n", __func__);

	_mbraink_memory_ops.getDdrInfo = ops->getDdrInfo;
	_mbraink_memory_ops.getMdvInfo = ops->getMdvInfo;
	_mbraink_memory_ops.get_ufs_info = ops->get_ufs_info;
	_mbraink_memory_ops.getEmiInfo = ops->getEmiInfo;
	_mbraink_memory_ops.getCmProfileInfo = ops->getCmProfileInfo;
	_mbraink_memory_ops.getVsmrInfo = ops->getVsmrInfo;
	_mbraink_memory_ops.getCmVoteInfo = ops->getCmVoteInfo;
	_mbraink_memory_ops.getCpuQosInfo = ops->getCpuQosInfo;
	_mbraink_memory_ops.getMMQosInfo = ops->getMMQosInfo;
	_mbraink_memory_ops.getCmDDRVoteInfo = ops->getCmDDRVoteInfo;

	return 0;
}
EXPORT_SYMBOL(register_mbraink_memory_ops);

int unregister_mbraink_memory_ops(void)
{
	pr_info("%s: unregister.\n", __func__);

	_mbraink_memory_ops.getDdrInfo = NULL;
	_mbraink_memory_ops.getMdvInfo = NULL;
	_mbraink_memory_ops.get_ufs_info = NULL;
	_mbraink_memory_ops.getEmiInfo = NULL;
	_mbraink_memory_ops.getCmProfileInfo = NULL;
	_mbraink_memory_ops.getVsmrInfo = NULL;
	_mbraink_memory_ops.getCmVoteInfo = NULL;
	_mbraink_memory_ops.getCpuQosInfo = NULL;
	_mbraink_memory_ops.getMMQosInfo = NULL;
	_mbraink_memory_ops.getCmDDRVoteInfo = NULL;

	return 0;
}
EXPORT_SYMBOL(unregister_mbraink_memory_ops);

int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	int ret = 0;

	if (pMemoryDdrInfo == NULL) {
		pr_info("%s: Ddr Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getDdrInfo)
		ret = _mbraink_memory_ops.getDdrInfo(pMemoryDdrInfo);
	else {
		pr_info("%s: Do not support ioctl getDdrInfo query.\n", __func__);
		pMemoryDdrInfo->totalDdrFreqNum = 0;
	}

	return ret;
}

int mbraink_memory_getMdvInfo(struct mbraink_memory_mdvInfo  *pMemoryMdv)
{
	int ret = 0;

	if (pMemoryMdv == NULL) {
		pr_info("%s: Mdv Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getMdvInfo)
		ret = _mbraink_memory_ops.getMdvInfo(pMemoryMdv);
	else
		pr_info("%s: Do not support ioctl getMdv query.\n", __func__);

	return ret;
}

int mbraink_get_ufs_info(struct mbraink_ufs_info *ufs_info)
{
	int ret = 0;

	if (_mbraink_memory_ops.get_ufs_info)
		ret = _mbraink_memory_ops.get_ufs_info(ufs_info);
	else
		pr_info("%s: Do not support ioctl get_ufs_info.\n", __func__);

	return ret;
}

int mbraink_memory_getEmiInfo(struct mbraink_memory_emiInfo *pMemoryEmiInfo)
{
	int ret = 0;

	if (pMemoryEmiInfo == NULL) {
		pr_info("%s: Emi Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getEmiInfo)
		ret = _mbraink_memory_ops.getEmiInfo(pMemoryEmiInfo);
	else {
		pr_info("%s: Do not support ioctl getEmiInfo query.\n", __func__);
		pMemoryEmiInfo->totalEmiFreqNum = 0;
	}

	return ret;
}

int mbraink_memory_getCmProfileInfo(struct mbraink_memory_cmProfileInfo *pCmProfileInfo)
{
	int ret = 0;

	if (pCmProfileInfo == NULL) {
		pr_info("%s: Cm Profile Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getCmProfileInfo)
		ret = _mbraink_memory_ops.getCmProfileInfo(pCmProfileInfo);
	else {
		pr_info("%s: Do not support ioctl getCmProfileInfo query.\n", __func__);
		pCmProfileInfo->totalCmWrapNum = 0;
	}

	return ret;
}

int mbraink_memory_getVsmrInfo(struct mbraink_memory_vsmrInfo *pVsmrInfo)
{
	int ret = 0;

	if (pVsmrInfo == NULL) {
		pr_info("%s: Vsmr Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getVsmrInfo)
		ret = _mbraink_memory_ops.getVsmrInfo(pVsmrInfo);
	else {
		pr_info("%s: Do not support ioctl getVsmrInfo query.\n", __func__);
		pVsmrInfo->vsmr_support = 0;
	}

	return ret;
}

int mbraink_memory_getCmVoteInfo(struct mbraink_memory_cmVoteInfo *pCmVoteInfo)
{
	int ret = 0;

	if (pCmVoteInfo == NULL) {
		pr_info("%s: Cm Vote Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getCmVoteInfo)
		ret = _mbraink_memory_ops.getCmVoteInfo(pCmVoteInfo);
	else
		pr_info("%s: Do not support ioctl getCmVoteInfo query.\n", __func__);

	return ret;
}

int mbraink_memory_getCpuQosInfo(struct mbraink_memory_cpuQosInfo *pCpuQosInfo)
{
	int ret = 0;

	if (pCpuQosInfo == NULL) {
		pr_info("%s: Cpu Qos Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getCpuQosInfo)
		ret = _mbraink_memory_ops.getCpuQosInfo(pCpuQosInfo);
	else {
		pr_info("%s: Do not support ioctl getCpuQosInfo query.\n", __func__);
		pCpuQosInfo->data_lv_length = 0;
	}

	return ret;
}

int mbraink_memory_getMMQosInfo(struct mbraink_mem_mmQosInfo *pMMQosInfo)
{
	int ret = 0;

	if (pMMQosInfo == NULL) {
		pr_info("%s: MM Qos Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getMMQosInfo)
		ret = _mbraink_memory_ops.getMMQosInfo(pMMQosInfo);
	else {
		pr_info("%s: Do not support ioctl getMMQosInfo query.\n", __func__);
		pMMQosInfo->subsys_num = 0;
	}

	return ret;
}

int mbraink_memory_getCmDDRVoteInfo(struct mbraink_memory_cmDDRVoteInfo *pCmDDRVoteInfo)
{
	int ret = 0;

	if (pCmDDRVoteInfo == NULL) {
		pr_info("%s: CM DDR Vote Info is null.\n", __func__);
		return -1;
	}

	if (_mbraink_memory_ops.getCmDDRVoteInfo)
		ret = _mbraink_memory_ops.getCmDDRVoteInfo(pCmDDRVoteInfo);
	else
		pr_info("%s: Do not support ioctl getCmDDRVoteInfo query.\n", __func__);

	return ret;
}
