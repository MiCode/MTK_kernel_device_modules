// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <mtk_spm_internal.h>
//#include <mt-plat/mtk_secure_api.h>

#if IS_ENABLED(CONFIG_MTK_PMIC) || IS_ENABLED(CONFIG_MTK_PMIC_NEW_ARCH)
//#include <mt-plat/upmu_common.h>
#endif

#include <mtk_spm.h>
#include <mtk_spm_idle.h>
#include <mtk_spm_sip.h>

#include <mtk_spm_pmic_wrap.h>
//#include "pmic_api_buck.h"
#include "mtk_spm_resource_req_internal.h"


void spm_sodi_pre_process(struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	spm_pmic_power_mode(PMIC_PWR_SODI, 0, 0);
#endif
}

void spm_sodi_post_process(void)
{

}

void spm_sodi3_pre_process(struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	spm_pmic_power_mode(PMIC_PWR_SODI3, 0, 0);
#endif
}

void spm_sodi3_post_process(void)
{

}

void spm_sodi_pcm_setup_before_wfi(
	u32 cpu, struct pcm_desc *pcmdesc, struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
	unsigned int resource_usage;

	spm_sodi_pre_process(pwrctrl, operation_cond);

	__spm_sync_pcm_flags(pwrctrl);

	/* Get SPM resource request and update reg_spm_xxx_req */
	resource_usage = spm_get_resource_usage();

	mt_secure_call(MTK_SIP_KERNEL_SPM_SODI_ARGS,
		pwrctrl->pcm_flags, resource_usage, pwrctrl->timer_val, 0);
}

void spm_sodi3_pcm_setup_before_wfi(
	u32 cpu, struct pcm_desc *pcmdesc, struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
	unsigned int resource_usage;

	spm_sodi3_pre_process(pwrctrl, operation_cond);

	__spm_sync_pcm_flags(pwrctrl);

	/* Get SPM resource request and update reg_spm_xxx_req */
	resource_usage = spm_get_resource_usage();

	mt_secure_call(MTK_SIP_KERNEL_SPM_SODI_ARGS,
		pwrctrl->pcm_flags, resource_usage, pwrctrl->timer_val, 0);
	mt_secure_call(MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS,
		SPM_PWR_CTRL_SODI3, PW_WDT_DISABLE, pwrctrl->wdt_disable, 0);
}


