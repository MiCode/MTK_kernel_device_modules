// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_cmdq_hyp.h"
#include "pkvm_cmdq_platform.h"
#include <asm/kvm_pkvm_module.h>
#include <pkvm_sys.h>
#include "mtk-larb-port.h"

#define GCED_BASE_PA 0x1e980000
#define GCEM_BASE_PA 0x1e990000
#define GCEM2_BASE_PA 0x1e9a0000

const struct pkvm_module_ops *pkvm_cmdq_plat_ops;
#define CALL_FROM_PLAT_OPS(fn, ...) pkvm_cmdq_plat_ops->fn(__VA_ARGS__)

void cmdq_set_plat_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_plat_ops = ops;
	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "enter");
}

static const int32_t cmdq_max_task_in_thread[CMDQ_MAX_SECURE_THREAD_COUNT] = {10, 10, 4, 10, 10};
int32_t cmdq_tz_get_max_task_in_thread(const int32_t thread)
{
	return cmdq_tz_is_a_secure_thread(thread) ?
		cmdq_max_task_in_thread[thread - CMDQ_MIN_SECURE_THREAD_ID] / CMDQ_MAX_SECURE_CORE_COUNT : 0;
}

static const int32_t cmdq_tz_cmd_block_size[CMDQ_MAX_SECURE_THREAD_COUNT] = {
	4 << 12, 4 << 12, 20 << 12, 4 << 12, 4 << 12};
int32_t cmdq_tz_get_cmd_block_size(const int32_t thread)
{
	return cmdq_tz_is_a_secure_thread(thread) ?
		cmdq_tz_cmd_block_size[thread - CMDQ_MIN_SECURE_THREAD_ID] : 0;
}

uint32_t cmdq_get_base_by_hwid(uint8_t hwid)
{
	if (hwid == 0)
		return GCED_BASE_PA;
	else if (hwid == 1)
		return GCEM_BASE_PA;
	else if (hwid == 2)
		return GCEM2_BASE_PA;
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_ERR "HWID not supported");
	CALL_FROM_PLAT_OPS(putx64, (u64)hwid);
	return 0;
}

uint8_t cmdq_get_hwid_by_base(uint32_t base)
{
	if (base == GCED_BASE_PA)
		return 0;
	else if (base == GCEM_BASE_PA)
		return 1;
	else if (base == GCEM2_BASE_PA)
		return 2;

	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_ERR "base not supported");
	CALL_FROM_PLAT_OPS(putx64, (u64)base);
	return 0;
}

uint32_t cmdq_secio_type(const uint32_t addr, const uint32_t cmdq_id)
{
	// CALL_FROM_PLAT_OPS(puts, __func__);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "addr:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)addr);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "cmdq_id:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)cmdq_id);

	switch (addr) {
	case CMDQ_SECIO_TYPE_RANGE_0
		... CMDQ_SECIO_TYPE_RANGE_1:
		if (cmdq_id == 0)
			return SECIO_GCE;
		else if (cmdq_id == 1)
			return SECIO_GCE_M;
		return SECIO_GCE_M2;
	default:
		return SECIO_MAX;
	}
	return SECIO_MAX;
}
int32_t cmdq_drv_imgsys_set_slc(void *data)
{
	return 0;
}

void cmdq_tz_mdp_handle(struct TaskStruct *pTask)
{
	CALL_FROM_PLAT_OPS(puts, __func__);
}

void cmdq_tz_assign_tzmp_command(struct TaskStruct *pTask)
{
#define MAE_SECURE_SUPPORT (1)
#define MAE_BASE	0x15310000

	if (pTask->hwid == 2 && pTask->thread == 9) { /* cmdq task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		cmdq_task_write_value_addr(pTask, GCEM2_BASE_PA + 0xbc, 0xfeedbeaf, UINT_MAX);
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
	} else if (pTask->hwid == 1 && pTask->thread == 10) { /* img task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT);

		/* imgsys dapc enable */
		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_ISP_IMGI;
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		/* Set imgsys GDomain and NSbit */
		cmdq_drv_imgsys_set_domain((void *)pTask, true);

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_SET);

		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT);

		/* imgsys dapc enable */
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		/* Clear imgsys GDomain and NSbit */
		cmdq_drv_imgsys_set_domain((void *)pTask, false);

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_SET);
	} else if (pTask->hwid == 1 && pTask->thread == 11) { /* aie task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		//dapc
		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_FDVT;
		cmdq_tz_set_dapc_security_reg(pTask, true, true); //enable DEVAPC

		cmdq_task_write_value_addr(pTask, MAE_BASE + 0x8200, 0x00A9, UINT_MAX);
		cmdq_task_write_value_addr(pTask, MAE_BASE + 0x8230, 0x0001, UINT_MAX);
		cmdq_tz_set_dapc_security_reg(pTask, false, true); //disable DEVAPC

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		//close sec

		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_FDVT;
		cmdq_tz_set_dapc_security_reg(pTask, true, true); //enable DEVAPC

		cmdq_task_write_value_addr(pTask, MAE_BASE + 0x8200, 0x0000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, MAE_BASE + 0x8230, 0x0000, UINT_MAX);
		cmdq_tz_set_dapc_security_reg(pTask, false, true); //disable DEVAPC

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
	}
}

void cmdq_task_cb(struct TaskStruct *pTask)
{
	if (pTask->throwAEE) {
		CALL_FROM_PLAT_OPS(puts, __func__);
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "hwid:");
		CALL_FROM_PLAT_OPS(putx64, (u64)pTask->hwid);
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "thread:");
		CALL_FROM_PLAT_OPS(putx64, (u64)pTask->thread);
	}
}

bool m4u_larb_port_without_aid(const uint32_t port)
{
	switch (port) {
	case M4U_LARB20_PORT0:
	case M4U_LARB20_PORT1:
	case M4U_LARB20_PORT2:
	case M4U_LARB20_PORT3:
		return true;
	default:
		return false;
	}
}

bool is_mdp_thread(const int32_t hwid, const int32_t thrd)
{
	return false;
}
