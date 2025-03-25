// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_cmdq_hyp.h"
#include "pkvm_cmdq_platform.h"
#include "sys.h"
#include <asm/kvm_pkvm_module.h>

#define GCED_BASE_PA 0x30100000
#define GCED2_BASE_PA 0x30300000
#define GCEM_BASE_PA 0x30180000
#define GCEM2_BASE_PA 0x30380000

static uint32_t gce_base_va;
static uint8_t cmdq_id;

const struct pkvm_module_ops *pkvm_cmdq_plat_ops;
#define CALL_FROM_PLAT_OPS(fn, ...) pkvm_cmdq_plat_ops->fn(__VA_ARGS__)

void cmdq_set_plat_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_plat_ops = ops;
	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "enter");
}

uint32_t cmdq_tz_get_gce_base_va(void)
{
	return gce_base_va;
}

void cmdq_tz_setup(uint8_t hwid)
{
	CALL_FROM_PLAT_OPS(puts, __func__);

	if (hwid == 0) {
		gce_base_va = GCED_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-d");
	} else if (hwid == 1) {
		gce_base_va = GCED2_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-d2");
	} else if (hwid == 2) {
		gce_base_va = GCEM_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-m");
	} else {
		gce_base_va = GCEM2_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-m2");
	}
	cmdq_id = hwid;
}

uint32_t cmdq_secio_type(const uint32_t addr, const uint32_t cmdq_id)
{
	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "addr:");
	CALL_FROM_PLAT_OPS(putx64, (u64)addr);
	switch (addr) {
	case CMDQ_SECIO_TYPE_RANGE_0
		... CMDQ_SECIO_TYPE_RANGE_1:
		if (cmdq_id == 0)
			return SECIO_GCE;
		else if (cmdq_id == 1)
			return SECIO_GCE_D2;
		else if (cmdq_id == 2)
			return SECIO_GCE_M;
		return SECIO_GCE_M2;
	case CMDQ_SECIO_TYPE_RANGE_7
		... CMDQ_SECIO_TYPE_RANGE_8:
		if (cmdq_id == 0)
			return SECIO_GCE_SECURITY;
		else if (cmdq_id == 1)
			return SECIO_GCE_D2_SECURITY;
		else if (cmdq_id == 2)
			return SECIO_GCE_M_SECURITY;
		return SECIO_GCE_M2_SECURITY;
	default:
		return SECIO_MAX;
	}
	return SECIO_MAX;
}

uint32_t cmdq_secio_read(const uint32_t addr)
{
	uint32_t secio_type = 0;
	uint32_t val;

	secio_type = cmdq_secio_type(CMDQ_SECIO_TYPE_GET_OFFSET(addr), cmdq_id);

	SECIO_READ(secio_type,
		CMDQ_SECIO_GET_OFFSET(addr), &val);

	return val;
}

void cmdq_secio_write(const uint32_t addr, const uint32_t val)
{
	uint32_t secio_type = 0;

	secio_type = cmdq_secio_type(CMDQ_SECIO_TYPE_GET_OFFSET(addr), cmdq_id);

	TZ_RESULT ret = SECIO_WRITE(secio_type,
		CMDQ_SECIO_GET_OFFSET(addr), val);

	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "ret:");
	CALL_FROM_PLAT_OPS(putx64, (u64)ret);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "secio_type:");
	CALL_FROM_PLAT_OPS(putx64, (u64)secio_type);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "addr:");
	CALL_FROM_PLAT_OPS(putx64, (u64)CMDQ_SECIO_GET_OFFSET(addr));
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "val:");
	CALL_FROM_PLAT_OPS(putx64, (u64)val);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "read:");
	CALL_FROM_PLAT_OPS(putx64, (u64)CMDQ_REG_GET32(addr));
}

int32_t cmdq_drv_imgsys_set_slc(void *data)
{
	struct TaskStruct *pTask= NULL;

	pTask = (struct TaskStruct *)data;
	cmdq_task_write_value_addr(pTask, (0x3400F000 + 0x000), 0x0000003C, UINT_MAX);
	cmdq_task_write_value_addr(pTask, (0x3400F000 + 0x004), 0x10430010, UINT_MAX);
	cmdq_task_write_value_addr(pTask, (0x3400F000 + 0x008), 0x10530010, UINT_MAX);
	cmdq_task_read(pTask, (0x3400F000 + 0x000), CMDQ_THR_SPR_IDX1);
	cmdq_task_read(pTask, (0x3400F000 + 0x004), CMDQ_THR_SPR_IDX2);
	cmdq_task_read(pTask, (0x3400F000 + 0x008), CMDQ_THR_SPR_IDX3);

	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "called");
	return 0;
}

void cmdq_tz_assign_tzmp_command(struct TaskStruct *pTask)
{
#define MAE_SECURE_SUPPORT (1)
#define MAE_BASE	0x34310000

	if (pTask->hwid == 3 && pTask->thread == 9) { /* cmdq task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		cmdq_task_write_value_addr(pTask, GCEM2_BASE_PA + 0xbc, 0xfeedbeaf, UINT_MAX);
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
	} else if (pTask->hwid == 2 && pTask->thread == 10) { /* img task */
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
	} else if (pTask->hwid == 2 && pTask->thread == 11) { /* aie task */
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
	} else if(pTask->hwid == 2 && pTask->thread == 8) {
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISC_WAIT);
		// devapc enable
		cmdq_drv_imgsys_set_slc((void *)pTask);
		// devapc disable
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISC_SET);
	}
}

void cmdq_task_cb(struct TaskStruct *pTask)
{
	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "hwid:");
	CALL_FROM_PLAT_OPS(putx64, (u64)pTask->hwid);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "thread:");
	CALL_FROM_PLAT_OPS(putx64, (u64)pTask->thread);

	if (pTask->hwid == 2 && pTask->thread == 8)
		cmdq_drv_imgsys_slc_cb();
}
