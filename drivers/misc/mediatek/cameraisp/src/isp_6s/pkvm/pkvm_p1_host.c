// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

 /*******************************************************************************
  * Includes
  ******************************************************************************/
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include "pkvm_p1_host.h"


/*******************************************************************************
 * MACRO Definitions
 ******************************************************************************/
#define LogTag "[PKVM_P1]"
#define LOG_NOTICE(format, args...)                                            \
	pr_notice(LogTag "[%s] " format, __func__, ##args)

#define CAM_PKVM_DEV_NAME "pkvm-p1"


/*******************************************************************************
 * MACRO Definitions
 ******************************************************************************/
static bool isPkvmSupport;
static int pkvm_p1_sec_config;
static int pkvm_p1_set_sec_cam;
static int pkvm_p1_set_dapc_auth;
static int pkvm_p1_set_dapc_reg;
static int pkvm_p1_APC_CamIspProtCtl;
static int pkvm_p1_get_sec_fh_info;
static int pkvm_p1_uninit;


/*******************************************************************************
 * IOCTL Handling
 ******************************************************************************/
static long pkvm_p1_ioctl(struct file *filep, unsigned int cmd, unsigned long Param)
{
	int ret = 0;

	LOG_NOTICE("cmd = 0x%x\n", cmd);

	switch (cmd) {
	case CAM_PKVM_SUPPORT:
	{
		LOG_NOTICE("CAM_PKVM_SUPPORT: isPkvmSupport=%d", isPkvmSupport);

		if (copy_to_user((void *)Param, &isPkvmSupport, sizeof(bool)) != 0) {
			LOG_NOTICE("ERROR: copy_to_user FAILED\n");
			ret = -EFAULT;
		}
		break;
	}
	case CAM_PKVM_GET_HWINFO:
	{
		struct SecMgr_QueryInfo_PKVM   queryinfo;

		if (likely(copy_from_user(&queryinfo, (void *)Param, sizeof(struct SecMgr_QueryInfo_PKVM)) == 0)) {
			LOG_NOTICE("CAM_PKVM_GET_HWINFO\n");

			queryinfo.Num_of_Cam = 1;
			queryinfo.CAM_CTL_DMA_EN = 0x280035;

			for (int i = 0; i < DAPC_NUM_CQ; i++)
				queryinfo.SecReg_ADDR_CQ[i] = gisp_drv_reg_addr[i];

			if (copy_to_user((void *)Param, &queryinfo, sizeof(struct SecMgr_QueryInfo_PKVM)) != 0) {
				LOG_NOTICE("ERROR: copy_to_user FAILED\n");
				ret = -EFAULT;
			}
		} else {
			LOG_NOTICE("CAM_PKVM_GET_HWINFO: copy_from_user FAILED\n");
			ret = -EFAULT;
		}
		break;
	}
	case CAM_PKVM_SEC_CONFIG:
	{
		uint32_t port;

		if (likely(copy_from_user(&port, (void *)Param, sizeof(uint32_t)) == 0)) {
			LOG_NOTICE("CAM_PKVM_SEC_CONFIG: port = %d\n", port);
			ret = pkvm_el2_mod_call(pkvm_p1_sec_config, port);
		} else {
			LOG_NOTICE("CAM_PKVM_SEC_CONFIG: copy_from_user FAILED\n");
			ret = -EFAULT;
		}

		break;
	}
	case CAM_PKVM_SET_DAPC_REG:
	{
		struct SecMgr_RegInfo_PKVM reginfo_pkvm;
		bool dapc_auth_result = false;
		bool isEnable = true;

		if (likely(copy_from_user(&reginfo_pkvm, (void *)Param, sizeof(struct SecMgr_RegInfo_PKVM)) == 0)) {
			LOG_NOTICE("CAM_PKVM_SET_DAPC_REG: CamModule=%d", reginfo_pkvm.CamModule);

			ret = pkvm_el2_mod_call(pkvm_p1_APC_CamIspProtCtl, reginfo_pkvm.CamModule, isEnable);
			dapc_auth_result = pkvm_el2_mod_call(pkvm_p1_set_dapc_auth, reginfo_pkvm.CamModule,
				reginfo_pkvm.dapc_cq[DAPC_IDX_REG_CAMCTL_R1_CAMCTL_DMA_EN],
				reginfo_pkvm.dapc_cq[DAPC_IDX_REG_CAMCTL_R1_CAMCTL_DMA2_EN],
				reginfo_pkvm.dapc_cq[DAPC_IDX_REG_CAMCTL_R1_CAMCTL_SEL],
				reginfo_pkvm.dapc_cq[DAPC_IDX_REG_CAMCTL_R1_LCES_OUT_SIZE]);

			if (dapc_auth_result == true) {
				for (int i = 0; i < DAPC_NUM_WRITE; i++) {
					LOG_NOTICE("Data: 0x%x\n", reginfo_pkvm.dapc_cq[i]);
					ret |= pkvm_el2_mod_call(pkvm_p1_set_dapc_reg,
						reginfo_pkvm.CamModule, i, reginfo_pkvm.dapc_cq[i]);
				}
			} else {
				LOG_NOTICE("Bypass dapc write registers\n");
			}
		} else {
			LOG_NOTICE("CAM_PKVM_SET_DAPC_REG: copy_from_user FAILED\n");
			ret = -EFAULT;
		}
		break;
	}
	case CAM_PKVM_SET_SEC_CAM:
	{
		uint32_t CamModule;
		bool isEnable = true;

		if (likely(copy_from_user(&CamModule, (void *)Param, sizeof(uint32_t)) == 0)) {
			LOG_NOTICE("CAM_PKVM_SET_SEC_CAM: CamModule=%d", CamModule);
			ret |= pkvm_el2_mod_call(pkvm_p1_set_sec_cam, CamModule);
			ret |= pkvm_el2_mod_call(pkvm_p1_APC_CamIspProtCtl, CamModule, isEnable);
		} else {
			LOG_NOTICE("CAM_PKVM_GET_HWINFO: copy_from_user FAILED\n");
			ret = -EFAULT;
		}
		break;
	}
	case CAM_PKVM_GET_FH_INFO:
	{
		struct SecMgr_SecInfo_PKVM secinfo_pkvm;
		uint32_t temp = 0;
		uint32_t fh_copy_size = 0;

		if (likely(copy_from_user(&secinfo_pkvm, (void *)Param, sizeof(struct SecMgr_SecInfo_PKVM)) == 0)) {
			LOG_NOTICE("CAM_PKVM_GET_FH_INFO: sec_pa:0x%llx, port:%d",
				secinfo_pkvm.sec_pa, secinfo_pkvm.port);
			if (secinfo_pkvm.port == PORT_IDX_IMGO)
				fh_copy_size = 14;
			else
				fh_copy_size = 13;

			for (int i = 0; i < fh_copy_size; i++) {
				temp = pkvm_el2_mod_call(pkvm_p1_get_sec_fh_info, secinfo_pkvm.sec_pa, i);
				secinfo_pkvm.sec_fhinfo[i] = temp;
				// LOG_NOTICE("sec_fhinfo[%d]: 0x%x", i, secinfo_pkvm.sec_fhinfo[i]);
			}
			if (copy_to_user((void *)Param, &secinfo_pkvm, sizeof(struct SecMgr_SecInfo_PKVM)) != 0) {
				LOG_NOTICE("ERROR: copy_to_user FAILED\n");
				ret = -EFAULT;
			}
		} else {
			LOG_NOTICE("CAM_PKVM_GET_FH_INFO: copy_from_user FAILED\n");
			ret = -EFAULT;
		}
		break;
	}
	case CAM_PKVM_UNINIT:
	{
		LOG_NOTICE("CAM_PKVM_UNINIT");
		ret = pkvm_el2_mod_call(pkvm_p1_uninit);
		break;
	}
	default:
		LOG_NOTICE("ERROR: no such ioctl cmd(%d)\n", cmd);
		ret = -1;
		break;
	}

	return ret;
}



/*******************************************************************************
 * Module Init
 ******************************************************************************/
static const struct file_operations pkvm_isp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pkvm_p1_ioctl,
};

static struct miscdevice pkvm_isp_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = CAM_PKVM_DEV_NAME,
	.fops = &pkvm_isp_fops,
};

// pkvm p1 hypervisor functions
static int p1_hvc_register(unsigned long token)
{
	pkvm_p1_sec_config = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_sec_config), token);
	pkvm_p1_set_sec_cam = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_set_sec_cam), token);
	pkvm_p1_set_dapc_auth = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_set_dapc_auth), token);
	pkvm_p1_set_dapc_reg = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_set_dapc_reg), token);
	pkvm_p1_APC_CamIspProtCtl = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_APC_CamIspProtCtl), token);
	pkvm_p1_get_sec_fh_info = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_get_sec_fh_info), token);
	pkvm_p1_uninit = pkvm_register_el2_mod_call(kvm_nvhe_sym(pkvm_p1_hyp_uninit), token);

	return 0;
}

static int __init pkvm_p1_init(void)
{
	unsigned long token;
	int ret;

	LOG_NOTICE(" +\n");

	if (!is_protected_kvm_enabled()) {
		LOG_NOTICE("INFO: pkvm is NOT supported!\n");
		isPkvmSupport = false;
		return 0;
	}
	LOG_NOTICE("INFO: pkvm is supported!\n");
	isPkvmSupport = true;

	ret = pkvm_load_el2_module(kvm_nvhe_sym(p1_hyp_init), &token);
	if (ret) {
		LOG_NOTICE("ERROR: failed to load pkvm p1 module, ret=%d\n", ret);
		return ret;
	}

	ret = p1_hvc_register(token);
	if (ret) {
		LOG_NOTICE("ERROR: failed to register p1 hvc, ret=%d\n", ret);
		return ret;
	}

	ret = misc_register(&pkvm_isp_dev);
	if (ret) {
		LOG_NOTICE("ERROR: failed to register pkvm_p1 device, ret=%d\n", ret);
		return ret;
	}

	LOG_NOTICE("- INFO: success to load pkvm p1 module\n");

	return 0;
}


module_init(pkvm_p1_init);
MODULE_LICENSE("GPL");
