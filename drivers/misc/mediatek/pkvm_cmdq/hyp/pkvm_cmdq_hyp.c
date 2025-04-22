// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/delay.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvm_cmdq_hyp.h"
#include "pkvm_cmdq_platform.h"
#include "trustzone.h"

#ifdef memset
#undef memset
#endif

const struct pkvm_module_ops *pkvm_cmdq_ops;
#define CALL_FROM_OPS(fn, ...) pkvm_cmdq_ops->fn(__VA_ARGS__)

void *reserved_mem_va_base;
uint64_t reserved_mem_pa_base;

bool mtkcam_security_cam_normal_preview_support;

static struct ContextStruct gCmdqContext;
static struct list_node gCmdqFreeTask[CMDQ_MAX_SECURE_CORE_COUNT][CMDQ_MAX_SECURE_THREAD_COUNT];

uint32_t dapc_base_pa[] = {
	DAPC_BASE,
#ifdef DAPC_BASE2
	DAPC_BASE2,
#endif
#ifdef DAPC_BASE3
	DAPC_BASE3,
#endif
};

struct cmdq_protect_engine mdp_dapc_engines[] = {
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_15),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_21),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_27),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_32),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_33),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_39),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_40),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_45),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_46),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_50),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_IMG_APB_S_56),
	MDP_DAPC_ASSIGN(CMDQ_SEC_FDVT, DAPC_IMG_APB_S_38),
};

static bool cmdq_tz_is_a_secure_thread(const int32_t thread)
{
	if ((thread >= CMDQ_MIN_SECURE_THREAD_ID) &&
		((CMDQ_MIN_SECURE_THREAD_ID + CMDQ_MAX_SECURE_THREAD_COUNT) > thread)) {
		return true;
	}
	return false;
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

void cmdq_secmem_IOVAQuery(uint64_t *iova)
{
#define SIO_GET_IOVA (0)
#define MTK_SIP_HYP_CMDQ_CONTROL	0xC2000835
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(MTK_SIP_HYP_CMDQ_CONTROL,
		SIO_GET_IOVA, 0, 0, 0, 0, 0, 0, &res);
	*iova = res.a1;
}

int32_t cmdq_pkvm_cmd_buffer_init(void)
{
	struct TaskStruct *pTask;
	uint64_t pa, iova;
	int i, j, core;
	uint32_t *va;

	pa = reserved_mem_pa_base;
	va = (uint32_t *)reserved_mem_va_base;

#if defined(CMDQ_IOVA)
	cmdq_secmem_IOVAQuery(&iova);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pre-alloc pa:");
	CALL_FROM_OPS(putx64, (u64)pa);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "iova:");
	CALL_FROM_OPS(putx64, (u64)iova);
#else
	iova = 0;
#endif

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pre-alloc max core:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_MAX_SECURE_CORE_COUNT);
	for (core = 0; core < CMDQ_MAX_SECURE_CORE_COUNT; core++) {
		for (i = 0, j = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++) {
			CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pre-alloc hwid:");
			CALL_FROM_OPS(putx64, (u64)core);
			CALL_FROM_OPS(puts, PFX_CMDQ_MSG "thd:");
			CALL_FROM_OPS(putx64, (u64)(CMDQ_MIN_SECURE_THREAD_ID + i));
			CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pa:");
			CALL_FROM_OPS(putx64, (u64)pa);
			CALL_FROM_OPS(puts, PFX_CMDQ_MSG "va:");
			CALL_FROM_OPS(putx64, (u64)va);
			CALL_FROM_OPS(puts, PFX_CMDQ_MSG "size:");
			CALL_FROM_OPS(putx64, (u64)cmdq_tz_get_cmd_block_size(CMDQ_MIN_SECURE_THREAD_ID + i));
			list_for_every_entry(
				&gCmdqFreeTask[core][i], pTask, struct TaskStruct, listEntry) {
				pTask->bufferSize = cmdq_tz_get_cmd_block_size(CMDQ_MIN_SECURE_THREAD_ID + i);
#if defined(CMDQ_IOVA)
				pTask->MVABase = iova;
				iova += pTask->bufferSize;
#else
				pTask->MVABase = pa;
#endif
				pTask->pVABase = va;
				pTask->PABase = pa;
				pa += pTask->bufferSize;
				va = (uint32_t *)((uintptr_t)va + (pTask->bufferSize));

				j += 1;
			}
		}
	}
	return 0;
}

struct ThreadStruct *cmdq_pkvm_get_thread_struct_by_id(int32_t thread)
{
	if (thread < 0 || thread >= CMDQ_MAX_THREAD_COUNT) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "get failed");
		return NULL;
	}

	struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);
	return pThread;
}

static void cmdqPkvmRemoveTaskByCookie(struct ThreadStruct *pThread, int32_t index,
	enum TASK_STATE_ENUM targetTaskState)
{
	if (index < 0 || index >= CMDQ_MAX_TASK_IN_THREAD_MAX) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Invalid index:");
		CALL_FROM_OPS(putx64, (u64)index);
		return;
	}

	// check task status to prevent double clean-up thread's taskcount
	if (pThread->pCurTask[index]->taskState != TASK_STATE_BUSY) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "remove task taskStatus err:");
		CALL_FROM_OPS(putx64, (u64)(pThread->pCurTask[index]->taskState));
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
		CALL_FROM_OPS(putx64, (u64)pThread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "task_slot:");
		CALL_FROM_OPS(putx64, (u64)index);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "targetTaskState:");
		CALL_FROM_OPS(putx64, (u64)targetTaskState);
		return;
	}

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "remove task slot:");
	CALL_FROM_OPS(putx64, (u64)index);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "targetTaskState:");
	CALL_FROM_OPS(putx64, (u64)targetTaskState);
	pThread->pCurTask[index]->taskState = targetTaskState;

	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;
}

static void cmdq_pkvm_release_task(struct TaskStruct *pTask)
{
	int8_t hwid;

	if (pTask == NULL) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "NULL task");
		return;
	}

	hwid = pTask->hwid;

	do {
		/* note CMD buffer life cycle aligns to path resource and */
		/* CMD mapping aligns task config in IPC thread */
		if (pTask->pVABase != 0) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "try release pTask");
			CALL_FROM_OPS(putx64, (u64)pTask);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "but CMD buffer mapping still exists, VABase:");
			CALL_FROM_OPS(putx64, (u64)pTask->pVABase);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "MVABase:");
			CALL_FROM_OPS(putx64, (u64)pTask->MVABase);
		}

		// be carefully that list parameter order is not same as linux version
		// secure world's list: list_add_tail(list, list_node)
		list_add_tail_new(&gCmdqFreeTask[hwid][pTask->thread - CMDQ_MIN_SECURE_THREAD_ID],
			&(pTask->listEntry));


		pTask->taskState = TASK_STATE_IDLE;
		pTask->thread	 = CMDQ_INVALID_THREAD;
	} while (0);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "TASK release done, pTask:");
	CALL_FROM_OPS(putx64, (u64)pTask);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "state:");
	CALL_FROM_OPS(putx64, (u64)pTask->taskState);
}

static void cmdqPkvmRemoveTaskByCookieAndRelease(
		struct TaskStruct *pTask, int32_t thread, uint32_t cookie,
		enum TASK_STATE_ENUM targetTaskState)
{
	struct ThreadStruct *pThread = cmdq_pkvm_get_thread_struct_by_id(thread);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "HACK: update task:");
	CALL_FROM_OPS(putx64, (u64)pTask);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "status:");
	CALL_FROM_OPS(putx64, (u64)targetTaskState);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "and release it, thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "cookie:");
	CALL_FROM_OPS(putx64, (u64)cookie);

	/* update to task done status, and remove it from thread array */
	if (pThread && cookie < cmdq_tz_get_max_task_in_thread(thread))
		cmdqPkvmRemoveTaskByCookie(pThread, (int32_t)cookie, targetTaskState);

	/* release pTask, and add to free list */
	cmdq_pkvm_release_task(pTask);
}

int32_t cmdq_pkvm_map_task_command_buffer_VA(struct TaskStruct *pTask)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "CMDQ_MAP: VA:");
	CALL_FROM_OPS(putx64, (u64)pTask->pVABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "PA:");
	CALL_FROM_OPS(putx64, (u64)pTask->PABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "size:");
	CALL_FROM_OPS(putx64, (u64)pTask->bufferSize);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "thread:");
	CALL_FROM_OPS(putx64, (u64)pTask->thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "cmd size:");
	CALL_FROM_OPS(putx64, (u64)pTask->commandSize);
	pTask->pCMDEnd = pTask->pVABase + (pTask->commandSize / sizeof(pTask->pVABase[0])) - 1;

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "CMDQ_MAP: cmd_end:");
	CALL_FROM_OPS(putx64, (u64)pTask->pCMDEnd);
	return 0;
}

static int cmdq_task_append_command(struct TaskStruct *task,
	uint16_t arg_c, uint16_t arg_b, uint16_t arg_a, uint8_t s_op,
	uint8_t arg_c_type, uint8_t arg_b_type, uint8_t arg_a_type, uint8_t op)
{
	struct cmdq_instruction *cmdq_inst =
		(struct cmdq_instruction *)(task->pCMDEnd + 1);

	if (task->commandSize >= cmdq_tz_get_cmd_block_size(task->thread))
		return -ENOMEM;

	cmdq_inst->op = op;
	cmdq_inst->arg_a_type = arg_a_type;
	cmdq_inst->arg_b_type = arg_b_type;
	cmdq_inst->arg_c_type = arg_c_type;
	cmdq_inst->s_op = s_op;
	cmdq_inst->arg_a = arg_a;
	cmdq_inst->arg_b = arg_b;
	cmdq_inst->arg_c = arg_c;

	task->commandSize += CMDQ_INST_SIZE;
	task->pCMDEnd += 2;

	return 0;
}

int cmdq_task_wfe(struct TaskStruct *task, uint16_t event)
{
	uint32_t arg_b;

	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	/*
	 * WFE arg_b
	 * bit 0-11: wait value
	 * bit 15: 1 - wait, 0 - no wait
	 * bit 16-27: update value
	 * bit 31: 1 - update, 0 - no update
	 */
	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}

int cmdq_task_set_event(struct TaskStruct *task, uint16_t event)
{
	uint32_t arg_b;

	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE;
	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}

int cmdq_task_jump(struct TaskStruct *task, int32_t offset)
{
	int64_t off = offset >> 3;

	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(off),
		CMDQ_GET_ARG_B(off), 0, 0, 0, 0, 0, CMDQ_CODE_JUMP);
}

int cmdq_task_jump_addr(struct TaskStruct *task,
	uint64_t addr)
{
	uint64_t to_addr = CMDQ_PKVM_REG_SHIFT_ADDR(addr);

	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(to_addr),
		CMDQ_GET_ARG_B(to_addr), 1, 0, 0, 0, 0, CMDQ_CODE_JUMP);
}

int cmdq_task_finalize_loop(struct TaskStruct *task)
{
#define CMDQ_EOC_IRQ_EN (0)
	int err = 0;

	err = cmdq_task_append_command(task, CMDQ_GET_ARG_C(CMDQ_EOC_IRQ_EN),
		CMDQ_GET_ARG_B(CMDQ_EOC_IRQ_EN), 0, 0, 0, 0, 0, CMDQ_CODE_EOC);
	if (err)
		return err;

	return cmdq_task_jump_addr(task, task->MVABase);
}

int cmdq_task_assign_command(struct TaskStruct *task,
	uint16_t reg_idx, uint32_t value)
{
	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), reg_idx,
		CMDQ_LOGIC_ASSIGN, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_LOGIC);
}

int cmdq_task_store_value(struct TaskStruct *task,
	uint16_t indirect_dst_reg_idx, uint16_t dst_addr_low,
	uint32_t value, uint32_t mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != UINT_MAX) {
		err = cmdq_task_append_command(task, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MOVE);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	return cmdq_task_append_command(task, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), dst_addr_low,
		indirect_dst_reg_idx, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, op);
}

int cmdq_task_read_addr(struct TaskStruct *task,
	uint64_t addr, uint16_t dst_reg_idx)
{
	int err = 0;

	err = cmdq_task_assign_command(task, CMDQ_SPR_FOR_TEMP,
		CMDQ_GET_ADDR_HIGH(addr));
	if (err != 0)
		return err;

	return cmdq_task_append_command(task, 0,
		CMDQ_GET_ADDR_LOW(addr), dst_reg_idx,
		CMDQ_SPR_FOR_TEMP, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_READ_S);
}

int cmdq_task_read(struct TaskStruct *task,
	uint64_t src_addr, uint16_t dst_reg_idx)
{
	return cmdq_task_read_addr(task, src_addr, dst_reg_idx);
}

int cmdq_task_write_value_addr(struct TaskStruct *task,
	uint64_t addr, uint32_t value, uint32_t mask)
{
	int err = 0;

	err = cmdq_task_assign_command(task, CMDQ_SPR_FOR_TEMP,
		CMDQ_GET_ADDR_HIGH(addr));
	if (err != 0)
		return err;

	if (addr < 0x20000) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "addr cmd failed:");
		CALL_FROM_OPS(putx64, (u64)addr);
	}

	return cmdq_task_store_value(task, CMDQ_SPR_FOR_TEMP,
		CMDQ_GET_ADDR_LOW(addr), value, mask);
}

int32_t cmdq_core_insert_security_instruction(struct TaskStruct *pTask,
	uint32_t regAddr, uint32_t value, uint32_t mask)
{
	const uint32_t originalSize = (uint32_t)pTask->commandSize;
	/* be careful that subsys encoding position is different among platforms */
	int32_t offset;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;
	int err = 0;

	err = cmdq_task_assign_command(pTask, CMDQ_SPR_FOR_TEMP,
		CMDQ_GET_ADDR_HIGH(regAddr));
	if (err != 0)
		return err;

	/* write with mask */
	if (mask != 0xFFFFFFFF) {
		err = cmdq_task_append_command(pTask, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MOVE);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	err = cmdq_task_append_command(pTask, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), CMDQ_GET_ADDR_LOW(regAddr),
		CMDQ_SPR_FOR_TEMP, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, op);
	if (err != 0)
		return err;

	/* calculate added command length */
	offset = (int32_t)pTask->commandSize - (int32_t)originalSize;

	return offset;
}

static uint16_t dapc_sys_cnt[DAPC_SYS_CNT + 1] = {
	0,
	CMDQ_DAPC_SYS1_CNT,
#ifdef CMDQ_DAPC_SYS2_CNT
	CMDQ_DAPC_SYS2_CNT,
#endif
#ifdef CMDQ_DAPC_SYS3_CNT
	CMDQ_DAPC_SYS3_CNT,
#endif
};

static uint32_t g_mask[CMDQ_MAX_DAPC_COUNT] = {0};
static uint32_t g_enables[CMDQ_MAX_DAPC_COUNT] = {0};
static uint8_t g_dapc_sys[CMDQ_MAX_DAPC_COUNT] = {0};
static uint32_t g_dapc_reg_offset[CMDQ_MAX_DAPC_COUNT] = {0};
static bool g_index[CMDQ_MAX_DAPC_COUNT] = {false};

int32_t cmdq_tz_set_dapc_security_reg(struct TaskStruct *task, bool enable, bool use_cmdq)
{
	uint64_t engine_flag = task->enginesNeedDAPC;
	uint32_t disabled = 0x0;
	uint32_t i, value = 0;
	int32_t offset = 0;
	uint32_t module_bit = 0x3;
	uint32_t enable_bit = 0x2;
	uint32_t sys_idx, dapc_offset;
	uint32_t count = ARRAY_SIZE(mdp_dapc_engines);

	for (i = 0; i < count; i++) {
		if (!(engine_flag & mdp_dapc_engines[i].engine_flag))
			continue;
		dapc_offset = 0;
		for (sys_idx = 0; sys_idx <= mdp_dapc_engines[i].sys; sys_idx++)
			dapc_offset += dapc_sys_cnt[sys_idx];
		dapc_offset += mdp_dapc_engines[i].dapc_reg_offset;
		g_dapc_reg_offset[dapc_offset] = mdp_dapc_engines[i].dapc_reg_offset;
		g_dapc_sys[dapc_offset] = mdp_dapc_engines[i].sys;
		if (mdp_dapc_engines[i].dapc_level == 2) /* 2nd dapc use 0x1 */
			g_mask[dapc_offset] |= 0x1 << mdp_dapc_engines[i].bit;
		else
			g_mask[dapc_offset] |=
				module_bit << mdp_dapc_engines[i].bit;
		g_enables[dapc_offset] |=
			enable_bit << mdp_dapc_engines[i].bit;
		g_index[dapc_offset] = true;
	}
	for (i = 0; i < CMDQ_MAX_DAPC_COUNT; i++) {
		//if (!g_mask[i])
		if (!g_index[i])
			continue;

		value = enable ? g_enables[i] : disabled;
		if (use_cmdq) {
			offset += cmdq_core_insert_security_instruction(
				task, DAPC_REG_PA(g_dapc_sys[i],
				g_dapc_reg_offset[i]), value, g_mask[i]);
		}
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "DAPC_REG_PA");
		CALL_FROM_OPS(putx64, (u64)DAPC_REG_PA(g_dapc_sys[i], g_dapc_reg_offset[i]));
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "value");
		CALL_FROM_OPS(putx64, (u64)value);
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "mask");
		CALL_FROM_OPS(putx64, (u64)g_mask[i]);
	}

	return offset;
}

void cmdqUtilPrintHexDump(const char *prefix_str, uint32_t *buf,
	uint32_t len, uint64_t pa)
{
	uint32_t i;
	struct cmdq_instruction *cmdq_inst;

	if (!buf)
		return;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "hex dump, buf:");
	CALL_FROM_OPS(putx64, (u64)buf);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR ", len: ");
	CALL_FROM_OPS(putx64, (u64)len);
	for (i = 0; i < (len/CMDQ_INST_SIZE); i++) {
		cmdq_inst = (struct cmdq_instruction *)(buf + i*2);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "cmd");
		CALL_FROM_OPS(putx64, pa + CMDQ_INST_SIZE * i);
		CALL_FROM_OPS(putx64, (u64)(buf + i*2));
		CALL_FROM_OPS(putx64, cmdq_inst->op);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_a_type);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_b_type);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_c_type);
		CALL_FROM_OPS(putx64, cmdq_inst->s_op);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_a);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_b);
		CALL_FROM_OPS(putx64, cmdq_inst->arg_c);
	}
}

struct TaskStruct *cmdq_pkvm_acquire_task_with_metadata(int32_t hwid_thrd,
		int32_t cookie, int32_t scenario)
{
	struct TaskStruct *pTask = NULL;
	struct ThreadStruct *pThread = NULL;
	uint32_t taskID, threadID;
	int32_t status, hwid, thread, max_task = 0;

	thread = CMDQ_GET_THREAD(hwid_thrd);
	hwid = CMDQ_GET_HWID(hwid_thrd);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "-->TASK: acquire hwid:");
	CALL_FROM_OPS(putx64, (u64)hwid);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "COOKIE:");
	CALL_FROM_OPS(putx64, (u64)cookie);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "SCENARIO:");
	CALL_FROM_OPS(putx64, (u64)scenario);

	do {
		pThread = cmdq_pkvm_get_thread_struct_by_id(thread);
		if (pThread == NULL)
			break;

		max_task = cmdq_tz_get_max_task_in_thread(thread);
		if (max_task == 0)
			return pTask;

		taskID = cookie % max_task;
		pTask = pThread->pCurTask[taskID];

		if (pTask == NULL)
			break;

		cmdqPkvmRemoveTaskByCookieAndRelease(pTask, thread, taskID, TASK_STATE_DONE);
	} while (0);

	threadID = thread - CMDQ_MIN_SECURE_THREAD_ID;
	pTask = list_peek_head_type(&gCmdqFreeTask[hwid][threadID], struct TaskStruct, listEntry);
	do {
		if (pTask == NULL) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "TASK: no free task, hwid:");
			CALL_FROM_OPS(putx64, (u64)hwid);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
			CALL_FROM_OPS(putx64, (u64)thread);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "gCmdqFreeTask:");
			CALL_FROM_OPS(putx64, (u64)&gCmdqFreeTask[hwid][threadID]);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "prev:");
			CALL_FROM_OPS(putx64, (u64)gCmdqFreeTask[hwid][threadID].prev);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "next:");
			CALL_FROM_OPS(putx64, (u64)gCmdqFreeTask[hwid][threadID].next);
			break;
		}

		if (pTask->bufferSize <= 0) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "TASK: invalid task");
			CALL_FROM_OPS(putx64, (u64)pTask);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "command buffer, size:");
			CALL_FROM_OPS(putx64, (u64)pTask->bufferSize);

			pTask = NULL;
			break;
		}

		/* init basic information */
		pTask->taskState  = TASK_STATE_WAITING;
		pTask->scenario   = scenario;
		pTask->hwid = hwid;
		pTask->thread	 = thread; /* note we dispatch thread form normal path */
		pTask->irqFlag	= 0;
		pTask->waitCookie = cookie;

		/* command buffer related */
		/* 1. map command buffer VA */
		status = cmdq_pkvm_map_task_command_buffer_VA(pTask);
		if (status < 0) {
			pTask = NULL;
			break;
		}
		/* 2. compose secure path command*/
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "size:");
		CALL_FROM_OPS(putx64, (u64)cmdq_tz_get_cmd_block_size(pTask->thread));
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "va base:");
		CALL_FROM_OPS(putx64, (u64)pTask->pVABase);

		CALL_FROM_OPS(memset, pTask->pVABase, 0, cmdq_tz_get_cmd_block_size(pTask->thread));

		pTask->commandSize = 0;
		pTask->pCMDEnd = pTask->pVABase - 1;

		cmdq_task_jump(pTask, CMDQ_INST_SIZE);
		cmdq_tz_assign_tzmp_command(pTask);
		cmdq_task_finalize_loop(pTask);

#if defined(CMDQ_DEBUG)
		cmdqUtilPrintHexDump("[CMDQ][pkvm]", pTask->pVABase, pTask->commandSize,
			pTask->MVABase);
#endif

		/* remove task form free list*/
		list_delete(&(pTask->listEntry));
	} while (0);

	if (pTask) {
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "<--TASK: acquire, pTask:0x");
		CALL_FROM_OPS(putx64, (u64)pTask);
	}

	return pTask;
}

struct ThreadStruct *cmdq_tz_get_thread_struct_by_id(const int32_t thread)
{
	if (thread < 0 || thread >= CMDQ_MAX_THREAD_COUNT) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "get failed");
		return NULL;
	}

	/* support one thread in secure world now
	 * the thread id is dispatched from normal world, and secure context
	 */
	struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);
	return pThread;
}

int32_t cmdq_pkvm_reset_HW_thread(int32_t thread)
{
	uint32_t loop = 0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: reset HW thread:");
	CALL_FROM_OPS(putx64, (u64)thread);

	CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);

	while (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread)) == 0x1) {
		if (loop > CMDQ_MAX_LOOP_COUNT) {
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
			CALL_FROM_OPS(putx64, (u64)thread);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "failed loop:");
			CALL_FROM_OPS(putx64, (u64)loop);

			return -(CMDQ_ERR_RESET_HW_FAILED);
		}
		loop++;
	}

	/* reset THR_EXEC_COUNT in shared DRAM */
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: reset HW thread");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "+ , CNT:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)));

	CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), 0);

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "- , CNT:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)));

	return 0;
}

static enum CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_tz_priority_from_scenario(enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		/* currently, a prefetch thread is always in high priority. */
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

		/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return CMDQ_THR_PRIO_DISPLAY_ESD;

	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERHIGH;

	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERLOW;

	default:
		/* other cases need exta logic, see below. */
		break;
	}

	if (scenario == CMDQ_SCENARIO_TRIGGER_LOOP)
		return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	else
		return CMDQ_THR_PRIO_NORMAL;
}

int32_t cmdq_pkvm_insert_task_from_thread_array_by_cookie(struct TaskStruct *pTask,
	struct ThreadStruct *pThread, const uint32_t cookie, const bool resetHWThread)
{
	int32_t max_task = 0;

	if (NULL == pTask || NULL == pThread) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "invalid param, pTask:");
		CALL_FROM_OPS(putx64, (u64)pTask);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "pThread:");
		CALL_FROM_OPS(putx64, (u64)pThread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "cookie:");
		CALL_FROM_OPS(putx64, (u64)cookie);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "needReset:");
		CALL_FROM_OPS(putx64, (u64)resetHWThread);
		return -(CMDQ_ERR_NULL_TASK);
	}

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: insert to thread:");
	CALL_FROM_OPS(putx64, (u64)pTask->thread);

	if (resetHWThread) {
		pThread->waitCookie = cookie;
		pThread->nextCookie = cookie + 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE - 1) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		/* taskCount must start from 0. */
		/* and we are the first task, so set to 1. */
		pThread->taskCount = 1;

	} else {
		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE - 1) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		pThread->taskCount++;
	}

	max_task = cmdq_tz_get_max_task_in_thread(pTask->thread);
	if (max_task != 0)
		pThread->pCurTask[cookie % (uint32_t)max_task] = pTask;

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "done");
	return 0;
}

static int32_t cmdq_pkvm_enable_HW_thread(int32_t thread)
{
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: enable thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "val:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread)));

	return 0;
}

int32_t cmdq_pkvm_exec_task_async(struct TaskStruct *pTask, int32_t hwid_thrd)
{
	struct ThreadStruct *pThread;
	int32_t status = 0, thread;
	uint32_t threadPrio;
	const int32_t cookie = pTask->waitCookie;

	thread = CMDQ_GET_THREAD(hwid_thrd);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "-->EXEC: task:");
	CALL_FROM_OPS(putx64, (u64)pTask);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "va:");
	CALL_FROM_OPS(putx64, (u64)pTask->pVABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pa:");
	CALL_FROM_OPS(putx64, (u64)pTask->MVABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "size:");
	CALL_FROM_OPS(putx64, (u64)pTask->commandSize);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "bufferSize:");
	CALL_FROM_OPS(putx64, (u64)pTask->bufferSize);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "scenario:");
	CALL_FROM_OPS(putx64, (u64)pTask->scenario);

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "cookie:");
	CALL_FROM_OPS(putx64, (u64)cookie);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "spr:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR0(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR1(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR2(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR3(thread)));
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "hw cookie:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_GET_COOKIE_CNT(thread));

	pThread = cmdq_tz_get_thread_struct_by_id(thread);
	if (!pThread) {
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "can't get thread struct by id:");
		CALL_FROM_OPS(putx64, (u64)thread);
		return -(CMDQ_ERR_INVALID_SECURITY_THREAD);
	}

	/* attach task to thread, so switch state to BUSY */
	pTask->taskState = TASK_STATE_BUSY;

	status = cmdq_pkvm_reset_HW_thread(thread);
	if (status < 0)
		return status;

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: new HW thread:");
	CALL_FROM_OPS(putx64, (u64)thread);

	/* NWd CMDQ will monitor task execute result, so we enable EXEC_CMD only */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread), CMDQ_THR_IRQ_FALG_EXEC_CMD | CMDQ_THR_IRQ_FALG_INVALID_INSTN);

	/* HW thread config */
	/* HACK: disable HW timeout */
	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), 0);	/* HACK: disable HW timeout */
	/* set THRx_SECURITY=1, THRx_SECURE_IRQ_EN=0 */
	CMDQ_REG_SET32(CMDQ_THR_SECURITY(thread), 0x1);		/* set THRx_SECURITY=1, THRx_SECURE_IRQ_EN=0 */
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "SECURITY:");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SECURITY(thread)));
	CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
		CMDQ_PKVM_REG_SHIFT_ADDR(pTask->MVABase + pTask->commandSize));
	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
		CMDQ_PKVM_REG_SHIFT_ADDR(pTask->MVABase));

	/* thread priority */
	threadPrio = cmdq_tz_priority_from_scenario((enum CMDQ_SCENARIO_ENUM)pTask->scenario);
	/* bit 0-2 for priority level; */
	CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7);	/* bit 0-2 for priority level; */

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "pc:");
	CALL_FROM_OPS(putx64, (u64)pTask->MVABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "qos:");
	CALL_FROM_OPS(putx64, (u64)threadPrio);

	/* attach pTask to the thread */
	cmdq_pkvm_insert_task_from_thread_array_by_cookie(pTask, pThread, (uint32_t)cookie, true);

	/* enable HW */
	cmdq_pkvm_enable_HW_thread(thread);

	return status;
}

int32_t cmdq_pkvm_handle_submit_task_async(int32_t hwid_thrd, int32_t waitCookie, int32_t scenario)
{
	struct TaskStruct *pTask = NULL;
	int32_t status = 0;

	// compose tzmp task
	pTask = cmdq_pkvm_acquire_task_with_metadata(hwid_thrd, waitCookie, scenario);

	if (pTask == NULL)
		return -CMDQ_ERR_NULL_TASK;

	// submit to GCE
	status = cmdq_pkvm_exec_task_async(pTask, hwid_thrd);
	if (status < 0) {
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Exec task async fail status:");
		CALL_FROM_OPS(putx64, (u64)status);
	}
	return status;
}

void cmdq_hyp_submit_task(struct user_pt_regs *regs)
{
	uint32_t hwid_thrd, waitCookie, scenario;
	int32_t status = 0;
	uint8_t hwid;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	hwid_thrd = regs->regs[1];
	waitCookie = regs->regs[2];
	scenario = regs->regs[3] & 0x7FFF;

	hwid = (uint8_t)CMDQ_GET_HWID(hwid_thrd);
	cmdq_tz_setup(hwid);
	status = cmdq_pkvm_handle_submit_task_async(hwid_thrd, waitCookie, scenario);

	if (status == 0)
		regs->regs[0] = SMCCC_RET_SUCCESS;
	else
		regs->regs[0] = status;
}

void cmdq_hyp_res_release(struct user_pt_regs *regs)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

int32_t cmdq_pkvm_suspend_HW_thread(int32_t thread)
{
	int retry = 0;

	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);

	while (!(CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread)) & 0x2)) {
		if (retry >= 100) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "retry:");
			CALL_FROM_OPS(putx64, (u64)retry);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "idx:");
			CALL_FROM_OPS(putx64, (u64)thread);
			break;
		}
		++retry;
	}

	return 0;
}

uint32_t *cmdq_tz_get_pc(const struct TaskStruct *pTask, uint32_t thread,
	uint32_t insts[4])
{
	u64 currPC = 0LL;
	uint8_t *pInst = NULL;

	insts[0] = 0;
	insts[1] = 0;
	insts[2] = 0;
	insts[3] = 0;

	currPC = CMDQ_PKVM_REG_REVERT_ADDR(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
	pInst = (uint8_t *) pTask->pVABase + (currPC - pTask->MVABase);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "[PC] hwid:");
	CALL_FROM_OPS(putx64, (u64)pTask->hwid);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thr:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "PC:");
	CALL_FROM_OPS(putx64, (u64)currPC);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "pVABase:");
	CALL_FROM_OPS(putx64, (u64)pTask->pVABase);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "MVABase:");
	CALL_FROM_OPS(putx64, (u64)pTask->MVABase);

	if (((uint8_t *) pTask->pVABase <= pInst) && (pInst <= (uint8_t *) pTask->pCMDEnd)) {
		if (pInst != (uint8_t *)pTask->pCMDEnd + 4) {
			/* If PC points to start of pCMD, */
			/* - 8 causes access violation */
			/* insts[0] = CMDQ_REG_GET32(pInst - 8); */
			/* insts[1] = CMDQ_REG_GET32(pInst - 4); */
			insts[2] = *(uint32_t *)(pInst + 0);
			insts[3] = *(uint32_t *)(pInst + 4);
		} else {
			/* insts[0] = CMDQ_REG_GET32(pInst - 16); */
			/* insts[1] = CMDQ_REG_GET32(pInst - 12); */
			insts[2] = *(uint32_t *)(pInst - 8);
			insts[3] = *(uint32_t *)(pInst - 4);
		}
	} else {
		/* invalid PC address */
		return NULL;
	}

	return (uint32_t *) pInst;
}

void cmdq_tz_dump_pc(const struct TaskStruct *pTask, int thread, const char *tag)
{
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };

	pcVA = cmdq_tz_get_pc(pTask, thread, insts);
	if (pcVA) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "hwid:");
		CALL_FROM_OPS(putx64, (u64)pTask->hwid);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "PC(VA):");
		CALL_FROM_OPS(putx64, (u64)pcVA);
		CALL_FROM_OPS(putx64, (u64)insts[2]);
		CALL_FROM_OPS(putx64, (u64)insts[3]);
	} else {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "hwid:");
		CALL_FROM_OPS(putx64, (u64)pTask->hwid);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "PC(VA): Not available\n");
	}

}

void cmdq_pkvm_attach_error_task(const struct TaskStruct *pTask, int32_t thread)
{
	struct ThreadStruct *pThread = NULL;
	uint32_t value[10] = { 0 };

	if (thread < 0 || thread >= CMDQ_MAX_THREAD_COUNT) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Invalid thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		return;
	}

	pThread = cmdq_pkvm_get_thread_struct_by_id(thread);
	if (!pThread)
		return;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Begin of Error");
	CALL_FROM_OPS(putx64, (u64)gCmdqContext.errNum);

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Error Thread(");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR ") Status");
	value[0] = (uint32_t)CMDQ_PKVM_REG_REVERT_ADDR(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
	value[1] = (uint32_t)CMDQ_PKVM_REG_REVERT_ADDR(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
	value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
	value[3] = CMDQ_GET_COOKIE_CNT(thread);
	value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
	value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
	value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
	value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
	value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));
	value[9] = CMDQ_REG_GET32(CMDQ_THR_SECURITY(thread));

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Enabled:");
	CALL_FROM_OPS(putx64, (u64)value[8]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "IRQ:");
	CALL_FROM_OPS(putx64, (u64)value[4]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Thread PC:");
	CALL_FROM_OPS(putx64, (u64)value[0]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "End:");
	CALL_FROM_OPS(putx64, (u64)value[1]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Wait Token:");
	CALL_FROM_OPS(putx64, (u64)value[2]);

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Curr Cookie:");
	CALL_FROM_OPS(putx64, (u64)value[3]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Wait Cookie:");
	CALL_FROM_OPS(putx64, (u64)pThread->waitCookie);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Next Cookie:");
	CALL_FROM_OPS(putx64, (u64)pThread->nextCookie);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Task Count:");
	CALL_FROM_OPS(putx64, (u64)pThread->taskCount);

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Timeout Cycle:");
	CALL_FROM_OPS(putx64, (u64)value[5]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Status:");
	CALL_FROM_OPS(putx64, (u64)value[6]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "IRQ_EN:");
	CALL_FROM_OPS(putx64, (u64)value[7]);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Secure:");
	CALL_FROM_OPS(putx64, (u64)value[9]);

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "spr ");
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR0(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR1(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR2(thread)));
	CALL_FROM_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR3(thread)));


	if (pTask != NULL) {
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Error Thread(");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR ") PC");
		cmdq_tz_dump_pc(pTask, thread, "ERR");

		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Error Task(");
		CALL_FROM_OPS(putx64, (u64)pTask);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR ") Status");

		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Task: ");
		CALL_FROM_OPS(putx64, (u64)pTask);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Scenario:");
		CALL_FROM_OPS(putx64, (u64)pTask->scenario);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "State:");
		CALL_FROM_OPS(putx64, (u64)pTask->taskState);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Priority:");
		CALL_FROM_OPS(putx64, (u64)pTask->priority);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "VABase:");
		CALL_FROM_OPS(putx64, (u64)pTask->pVABase);

		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Error Command(");
		CALL_FROM_OPS(putx64, (u64)pTask);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR ") Buffer");
		cmdqUtilPrintHexDump("[CMDQ][tz]", pTask->pVABase,
			(uint32_t)pTask->commandSize, pTask->MVABase);
	}

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "End of Error ");
	CALL_FROM_OPS(putx64, (u64)gCmdqContext.errNum);

	gCmdqContext.errNum++;
}

void cmdq_pkvm_remove_all_task_in_thread_unlocked(int32_t thread)
{
	struct ThreadStruct *pThread = NULL;
	struct TaskStruct *pTask   = NULL;
	int32_t index = 0;

	if (thread < CMDQ_MIN_SECURE_THREAD_ID ||
		thread >= CMDQ_MAX_SECURE_THREAD_COUNT + CMDQ_MIN_SECURE_THREAD_ID) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "Invalid thread:");
		CALL_FROM_OPS(putx64, (u64)thread);
		return;
	}

	do {
		pThread = cmdq_pkvm_get_thread_struct_by_id(thread);
		if (!pThread)
			return;

		for (index = 0; index < cmdq_tz_get_max_task_in_thread(thread); ++index) {

			pTask = pThread->pCurTask[index];
			if (pTask == NULL)
				continue;

			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "RELEASE_ALL_TASK: release task");
			CALL_FROM_OPS(putx64, (u64)pTask);
			pTask->irqFlag = 0xDEADDEAD; // unknown state;

			cmdqPkvmRemoveTaskByCookieAndRelease(
				pTask, thread, (uint32_t)index, TASK_STATE_ERROR);
		}
		pThread->taskCount = 0;
		pThread->waitCookie = pThread->nextCookie;
	} while (0);
}

int32_t cmdq_pkvm_disable_HW_thread(int32_t thread)
{
	cmdq_pkvm_reset_HW_thread(thread);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EXEC: disable HW thread:");
	CALL_FROM_OPS(putx64, (u64)thread);
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x00);
	return 0;
}

int32_t cmdq_pkvm_get_and_update_CNT_to_shared_CNT_region(int32_t thread)
{
	const int32_t cookie = (int32_t)CMDQ_GET_COOKIE_CNT(thread);

	CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), (uint32_t)cookie);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "SHARED_CNT: update thread");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "shared_cookie");
	CALL_FROM_OPS(putx64, (u64)cookie);

	return cookie;
}

void cmdq_hyp_cancel_task(struct user_pt_regs *regs)
{
	uint32_t hwid_thrd, scenario_aee;
	int32_t hwid, thread, max_task = 0;
	uint32_t currCookie, cookie;
	struct ThreadStruct *pThread = NULL;
	struct TaskStruct  *pTask = NULL;
	bool throwAEE;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	hwid_thrd = regs->regs[1];
	cookie = regs->regs[2];
	scenario_aee = regs->regs[3];
	throwAEE = ((scenario_aee & 0x8000) == 0x8000);

	thread = CMDQ_GET_THREAD(hwid_thrd);
	hwid = CMDQ_GET_HWID(hwid_thrd);

	cmdq_tz_setup((uint8_t)hwid);
	cmdq_pkvm_suspend_HW_thread(thread);

	do {
		pThread = cmdq_pkvm_get_thread_struct_by_id(thread);
		if (!pThread)
			return;
		max_task = cmdq_tz_get_max_task_in_thread(thread);
		if (max_task == 0)
			return;
		pTask = pThread->pCurTask[cookie % max_task];
		currCookie = (uint32_t)cmdq_pkvm_get_and_update_CNT_to_shared_CNT_region(thread);
		if (pTask == NULL) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "CANCEL_TASK: thread");
			CALL_FROM_OPS(putx64, (u64)thread);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "pCurTask:");
			CALL_FROM_OPS(putx64, (u64)cookie);
			CALL_FROM_OPS(puts, PFX_CMDQ_ERR "is NULL task");
			break;
		}

		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "CANCEL_TASK: start to remove all tasks on thread ");
		CALL_FROM_OPS(putx64, (u64)thread);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "since error task(cookie:");
		CALL_FROM_OPS(putx64, (u64)cookie);
		CALL_FROM_OPS(puts, PFX_CMDQ_ERR "), currCookie:");
		CALL_FROM_OPS(putx64, (u64)currCookie);

#if defined(CMDQ_DEBUG)
		cmdqUtilPrintHexDump("[CMDQ][pkvm]", pTask->pVABase, pTask->commandSize,
			pTask->MVABase);
#endif
		/* dump error */
		if (throwAEE)
			cmdq_pkvm_attach_error_task(pTask, thread);

		/* call user cb */
		cmdq_task_cb(pTask);

		/* remove all task in thread */
		cmdq_pkvm_remove_all_task_in_thread_unlocked(thread);
	} while (0);

	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "CANCEL_TASK: clear thread ");
	CALL_FROM_OPS(putx64, (u64)thread);
	CALL_FROM_OPS(puts, PFX_CMDQ_ERR "shared_cookie to 0");

	CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), 0);

	cmdq_pkvm_disable_HW_thread(thread);

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

void cmdq_hyp_path_res_allocate(struct user_pt_regs *regs)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	cmdq_pkvm_cmd_buffer_init();
	regs->regs[0] = SMCCC_RET_SUCCESS;
}

void cmdq_hyp_path_res_release(struct user_pt_regs *regs)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

void cmdq_hyp_pkvm_init(struct user_pt_regs *regs)
{
	struct TaskStruct *pTask;
	int32_t index, i, j, core;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	/* Reset overall context */
	CALL_FROM_OPS(memset, &gCmdqContext, 0, sizeof(struct ContextStruct));

	pTask = gCmdqContext.taskInfo;
	for (core = 0, index = 0; core < CMDQ_MAX_SECURE_CORE_COUNT; core++) {
		for (i = 0; i < CMDQ_MAX_SECURE_THREAD_COUNT; i++) {
			list_initialize(&gCmdqFreeTask[core][i]);
			for (j = 0; j < cmdq_tz_get_max_task_in_thread(CMDQ_MIN_SECURE_THREAD_ID + i); j++, index++) {
				list_initialize(&(pTask[index].listEntry));
				CALL_FROM_OPS(memset, &pTask[index], 0, sizeof(struct TaskStruct));

				pTask[index].taskState = TASK_STATE_IDLE;
				/* Init CMD buffer when setup secure path resource*/
				pTask[index].pVABase = NULL;
				pTask[index].pCMDEnd = NULL;
				pTask[index].MVABase = 0; /* Delay CMD buffer init when setup path resource */

				// be carefully that list parameter order is not same as linux version
				// secure world's list: list_add_tail(list, list_node)
				list_add_tail_new(&gCmdqFreeTask[core][i], &(pTask[index].listEntry));
			}
		}
	}

	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "<--init: core done");

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

void cmdq_hyp_pkvm_disable(struct user_pt_regs *regs)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

int cmdq_hyp_init(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_ops = ops;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");
	cmdq_set_plat_ops(ops);
	cmdq_set_isp_ops(ops);

	return 0;
}

void cmdq_hyp_get_memory(struct user_pt_regs *regs)
{
	uint64_t mem_base_address = regs->regs[1];
	uint64_t mem_size = regs->regs[2];
	unsigned long private_mappings_va = 0UL;
	int ret = 0;

	ret = pkvm_cmdq_ops->create_private_mapping(mem_base_address, mem_size,
				  PAGE_HYP | KVM_PGTABLE_PROT_NORMAL_NC, &private_mappings_va);
	if (!ret) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "enter");

		reserved_mem_va_base = (void *)private_mappings_va;
		reserved_mem_pa_base = mem_base_address;
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "reserved_mem_va:");
		CALL_FROM_OPS(putx64, (u64)reserved_mem_va_base);
	}

	ret = pkvm_cmdq_ops->host_stage2_mod_prot(mem_base_address >> PAGE_SHIFT, 0,
				  (mem_size / 2) >> PAGE_SHIFT, false);
	if (!ret) {
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EL1S2 unmap success");
	} else {
		CALL_FROM_OPS(puts, PFX_CMDQ_MSG "EL1S2 unmap fail, ret:");
		CALL_FROM_OPS(putx64, ret);
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
}

void cmdq_hyp_cam_preview_support(struct user_pt_regs *regs)
{
	mtkcam_security_cam_normal_preview_support = regs->regs[1];
	CALL_FROM_OPS(puts, PFX_CMDQ_MSG "mtkcam_security_cam_normal_preview_support:");
	CALL_FROM_OPS(putx64, mtkcam_security_cam_normal_preview_support);
	regs->regs[0] = SMCCC_RET_SUCCESS;
}
