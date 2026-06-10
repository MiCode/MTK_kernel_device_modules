// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "cmdq_sec_pkvm.h"

s32 cmdq_sec_pkvm_allocate_shared_memory(struct cmdq_sec_pkvm_context *tee,
	const dma_addr_t MVABase, const u32 size)
{
	return 0;
}

s32 cmdq_sec_pkvm_get_reply(struct cmdq_sec_pkvm_context *tee)
{
	cmdq_msg("%s rsp:%llx", __func__, tee->rsp);
	if (tee->rsp != 0)
		return -(tee->rsp);

	return 0;
}

s32 cmdq_sec_pkvm_allocate_wsm(struct cmdq_sec_pkvm_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2)
{
	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	*wsm_buffer = kzalloc(size, GFP_KERNEL);
	if (!*wsm_buffer) {
		cmdq_err("wsm_buffer fail");
		return -ENOMEM;
	}

	*wsm_buf_ex = kzalloc(size_ex, GFP_KERNEL);
	if (!*wsm_buf_ex) {
		cmdq_err("wsm_buf_ex fail");
		return -ENOMEM;
	}

	*wsm_buf_ex2 = kzalloc(size_ex2, GFP_KERNEL);
	if (!*wsm_buf_ex2) {
		cmdq_err("wsm_buf_ex2 fail");
		return -ENOMEM;
	}

	return 0;
}

s32 cmdq_sec_pkvm_free_wsm(struct cmdq_sec_pkvm_context *tee,
	void **wsm_buffer, void **wsm_buf_ex, void **wsm_buf_ex2)
{
	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	kfree(*wsm_buffer);
	*wsm_buffer = NULL;

	kfree(*wsm_buf_ex);
	*wsm_buf_ex = NULL;

	kfree(*wsm_buf_ex2);
	*wsm_buf_ex2 = NULL;

	return 0;
}

s32 cmdq_sec_pkvm_execute_session(struct cmdq_sec_pkvm_context *tee,
	u32 cmd, s32 timeout_ms, u32 thread_idx,
	u32 wait_cookie, s32 scenario_aee)
{
	struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	unsigned long smc_id = -1;
	unsigned long hvc_id;
#endif

	cmdq_mbox_mtcmos_by_fast(NULL, true);
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	switch (cmd + CMDQ_SMC_REQ_MAX) {
	case CMD_CMDQ_TL_SUBMIT_TASK:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_SUBMIT_TASK;
		break;
	case CMD_CMDQ_TL_RES_RELEASE:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_RES_RELEASE;
		break;
	case CMD_CMDQ_TL_CANCEL_TASK:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_CANCEL_TASK;
		break;
	case CMD_CMDQ_TL_PATH_RES_ALLOCATE:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_PATH_RES_ALLOCATE;
		break;
	case CMD_CMDQ_TL_PATH_RES_RELEASE:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_PATH_RES_RELEASE;
		break;
	default:
		cmdq_err("invalid cmd");
		break;
	}
	arm_smccc_1_1_smc(smc_id, 0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	pkvm_el2_mod_call(hvc_id, thread_idx, wait_cookie, scenario_aee);
	// tee->rsp = res.a0;
#else
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL, cmd + CMDQ_SMC_REQ_MAX,
		thread_idx, wait_cookie, scenario_aee, 0, 0, 0, &res);
	tee->rsp = res.a0;
#endif
	cmdq_mbox_mtcmos_by_fast(NULL, false);

	cmdq_msg("%s: cmd:%u CMDQ_SMC_REQ_MAX:%u rsp:%llx",
		__func__, cmd, CMDQ_SMC_REQ_MAX, tee->rsp);

	return 0;
}

s32 cmdq_sec_pkvm_execute_session_iwc(struct cmdq_sec_pkvm_context *tee, u32 cmd,
	void *iwc_msg, u32 size, void *iwc_ex1, u32 size_ex, void *iwc_ex2, u32 size_ex2)
{
	struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	unsigned long smc_id = -1;
	unsigned long hvc_id;
#endif
	u64 iwc_msg_pa, iwc_ex1_pa, iwc_ex2_pa;

	cmdq_mbox_mtcmos_by_fast(NULL, true);
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	switch (cmd) {
	case CMD_CMDQ_TL_SUBMIT_TASK_IWC:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_PKVM_IWC;
		break;
	case CMD_CMDQ_TL_SHARE_MEM_TO_EL2:
		smc_id = SMC_ID_MTK_PKVM_CMDQ_PKVM_SHARE_MEM;
		break;
	default:
		cmdq_err("invalid cmd");
		break;
	}
	arm_smccc_1_1_smc(smc_id, 0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	iwc_msg_pa = (u64)virt_to_phys(iwc_msg);
	iwc_ex1_pa = (u64)virt_to_phys(iwc_ex1);
	iwc_ex2_pa = (u64)virt_to_phys(iwc_ex2);

	pkvm_el2_mod_call(hvc_id, iwc_msg_pa, size, iwc_ex1_pa, size_ex, iwc_ex2_pa, size_ex2);
#endif
	cmdq_mbox_mtcmos_by_fast(NULL, false);

	cmdq_msg("%s: cmd:%u CMDQ_SMC_REQ_MAX:%u rsp:%llx",
		__func__, cmd, CMDQ_SMC_REQ_MAX, tee->rsp);

	return 0;
}

s32 cmdq_sec_pkvm_open_session(void)
{
	struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	unsigned long hvc_id;

	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_CMDQ_PKVM_INIT,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	pkvm_el2_mod_call(hvc_id);
#else
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL,
		CMD_CMDQ_TL_PKVM_INIT, 0, 0, 0, 0, 0, 0, &res);
#endif

	return 0;
}

s32 cmdq_sec_pkvm_send_metadata(const u32 meta_0, const u32 meta_1, const u32 meta_2,
	const u32 meta_3, const u32 meta_4, const u32 meta_5)
{
	struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_MTK_PKVM_CMDQ)
	unsigned long hvc_id;

	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_CMDQ_PKVM_INIT,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	pkvm_el2_mod_call(meta_0, meta_1, meta_2, meta_3, meta_4, meta_5);
#else
	arm_smccc_smc(MTK_SIP_CMDQ_CONTROL,
		CMD_CMDQ_TL_PKVM_INIT, meta_0, meta_1, meta_2,
		meta_3, meta_4, meta_5, &res);
#endif

	return 0;
}

MODULE_LICENSE("GPL");
