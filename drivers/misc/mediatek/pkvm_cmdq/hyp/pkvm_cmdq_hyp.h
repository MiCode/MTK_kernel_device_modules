/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __PKVM_CMDQ_HYP_H__
#define __PKVM_CMDQ_HYP_H__
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <asm/kvm_pkvm_module.h>
#include "isp_sec_public.h"
#include <pkvm_trustzone.h>
#define PFX_CMDQ_MSG "[cmdq] "
#define PFX_CMDQ_ERR "[cmdq][err] "
#include "list.h"

#define CMDQ_IWC_MAX_ADDR_LIST_LENGTH (30)


#define CMDQ_MAX_SECURE_THREAD_COUNT	(5) // support executing secure task
#define CMDQ_MIN_SECURE_THREAD_ID	(8)
#define CMDQ_MAX_TASK_IN_THREAD_MAX	(10)
#define CMDQ_MAX_FIXED_TASK		(44) // sum of cmdq_max_task_in_thread
#define CMDQ_MAX_THREAD_COUNT           (16)
#define CMDQ_INVALID_THREAD             (-1)
#define CMDQ_MAX_LOOP_COUNT             (1000000)
#define CMDQ_MAX_COOKIE_VALUE           (0xFFFFFFFF)

#define CMDQ_EVENT_MAX			0x3FF
#define CMDQ_IMMEDIATE_VALUE		0
#define CMDQ_REG_TYPE			1

#define CMDQ_GET_HWID(val)	((val >> 5) & 0x03)
#define CMDQ_GET_THREAD(val)	(val & 0x1F)
#define CMDQ_GET_ADDR_HIGH(addr)	((uint32_t)(addr >> 16))
#define CMDQ_ADDR_LOW_BIT		0x2
#define CMDQ_GET_ADDR_LOW(addr)		((uint16_t)addr | CMDQ_ADDR_LOW_BIT)
#define CMDQ_GET_ARG_B(arg)		((uint16_t)(arg >> 16))
#define CMDQ_GET_ARG_C(arg)		((uint16_t)arg)

#define CMDQ_REG_GET32(base, addr)        (cmdq_secio_read(base, addr))
#define CMDQ_REG_SET32(base, addr, val)   (cmdq_secio_write(base, addr, val))

/* CMDQ THRx */
#define CMDQ_CURR_IRQ_STATUS(base)         (base + 0x010)
#define CMDQ_SECURE_IRQ_STATUS(base)       (base + 0x014)
#define CMDQ_CURR_LOADED_THR(base)         (base + 0x018)
#define CMDQ_THR_SLOT_CYCLES(base)         (base + 0x030)
#define CMDQ_THR_EXEC_CYCLES(base)         (base + 0x034)
#define CMDQ_THR_TIMEOUT_TIMER(base)       (base + 0x038)
#define CMDQ_BUS_CONTROL_TYPE(base)        (base + 0x040)
#define CMDQ_CURR_INST_ABORT(base)         (base + 0x020)
#define CMDQ_SECURITY_ABORT(base)          (base + 0x050)

#define CMDQ_SECURITY_STA(base, id)        (base + (0x030 * id) + 0x024)
#define CMDQ_SECURITY_SET(base, id)        (base + (0x030 * id) + 0x028)
#define CMDQ_SECURITY_CLR(base, id)        (base + (0x030 * id) + 0x02C)

#define CMDQ_SYNC_TOKEN_ID(base)           (base + 0x060)
#define CMDQ_SYNC_TOKEN_VAL(base)          (base + 0x064)
#define CMDQ_SYNC_TOKEN_UPD(base)          (base + 0x068)

#define CMDQ_GPR_R32(base, id)             (base + (0x004 * id) + 0x80)

#define CMDQ_THR_WARM_RESET(base, id)      (base + (0x080 * id) + 0x100)
#define CMDQ_THR_ENABLE_TASK(base, id)     (base + (0x080 * id) + 0x104)
#define CMDQ_THR_SUSPEND_TASK(base, id)    (base + (0x080 * id) + 0x108)
#define CMDQ_THR_CURR_STATUS(base, id)     (base + (0x080 * id) + 0x10C)
#define CMDQ_THR_IRQ_STATUS(base, id)      (base + (0x080 * id) + 0x110)
#define CMDQ_THR_IRQ_ENABLE(base, id)      (base + (0x080 * id) + 0x114)
#define CMDQ_THR_CURR_ADDR(base, id)       (base + (0x080 * id) + 0x120)
#define CMDQ_THR_END_ADDR(base, id)        (base + (0x080 * id) + 0x124)
#define CMDQ_THR_EXEC_CNT(base, id)        (base + (0x080 * id) + 0x128)
#define CMDQ_THR_WAIT_TOKEN(base, id)      (base + (0x080 * id) + 0x130)
#define CMDQ_THR_CFG(base, id)             (base + (0x080 * id) + 0x140)
#define CMDQ_THR_INST_CYCLES(base, id)     (base + (0x080 * id) + 0x150)
#define CMDQ_THR_INST_THRESX(base, id)     (base + (0x080 * id) + 0x154)

#define CMDQ_THR_SPR0(base, id)	(base + (0x080 * id) + 0x160)
#define CMDQ_THR_SPR1(base, id)	(base + (0x080 * id) + 0x164)
#define CMDQ_THR_SPR2(base, id)	(base + (0x080 * id) + 0x168)
#define CMDQ_THR_SPR3(base, id)	(base + (0x080 * id) + 0x16c)

#define CMDQ_GET_COOKIE_CNT(base, thd) \
	(CMDQ_REG_GET32(base, CMDQ_THR_EXEC_CNT(base, thd)) & CMDQ_MAX_COOKIE_VALUE)

extern bool mtkcam_security_cam_normal_preview_support;

#define CMDQ_IMMEDIATE_VALUE		0
#define CMDQ_REG_TYPE			1
#define CMDQ_OPERAND_GET_IDX_VALUE(operand) \
	((operand)->reg ? (operand)->idx : (operand)->value)
#define CMDQ_OPERAND_TYPE(operand) \
	((operand)->reg ? CMDQ_REG_TYPE : CMDQ_IMMEDIATE_VALUE)

struct cmdq_instruction {
	uint16_t arg_c:16;
	uint16_t arg_b:16;
	uint16_t arg_a:16;
	uint8_t s_op:5;
	uint8_t arg_c_type:1;
	uint8_t arg_b_type:1;
	uint8_t arg_a_type:1;
	uint8_t op:8;
};

enum CMDQ_THR_IRQ_FLAG_ENUM {
	CMDQ_THR_IRQ_FALG_EXEC_CMD = 0x01, /* trigger IRQ if CMD executed done */
	CMDQ_THR_IRQ_FALG_INSTN_TIMEOUT  = 0x02, /* trigger IRQ if instuction timeout */
	CMDQ_THR_IRQ_FALG_INVALID_INSTN  = 0x10
};

enum TASK_STATE_ENUM {
	TASK_STATE_IDLE	   = 0,	/* free task */
	TASK_STATE_BUSY	   = 1,	/* task running on a thread */
	TASK_STATE_KILLED  = 2,	/* task process being killed */
	TASK_STATE_ERROR   = 3,	/* task execution error */
	TASK_STATE_DONE	   = 4,	/* task finished */
	TASK_STATE_WAITING = 5,	/* allocated but waiting for available thread */
	TASK_STATE_MDP_RDY = 6,	/* allocated but waiting for available thread */
};

enum CMDQ_HW_THREAD_PRIORITY_ENUM {
	CMDQ_THR_PRIO_SUPERLOW = 0,	/* low priority monitor loop */

	CMDQ_THR_PRIO_NORMAL = 1,	/* nomral priority */
	CMDQ_THR_PRIO_DISPLAY_TRIGGER = 2,	/* trigger loop (enables display mutex) */

	/* display ESD check (every 2 secs) */
	CMDQ_THR_PRIO_DISPLAY_ESD = 4,

	CMDQ_THR_PRIO_DISPLAY_CONFIG = 4,	/* display config (every frame) */

	CMDQ_THR_PRIO_SUPERHIGH = 5,	/* High priority monitor loop */

	CMDQ_THR_PRIO_MAX = 7,	/* maximum possible priority */
};

enum CMDQ_SCENARIO_ENUM {
	CMDQ_SCENARIO_JPEG_DEC = 0,
	CMDQ_SCENARIO_PRIMARY_DISP = 1,
	CMDQ_SCENARIO_PRIMARY_MEMOUT = 2,
	CMDQ_SCENARIO_PRIMARY_ALL = 3,
	CMDQ_SCENARIO_SUB_DISP = 4,
	CMDQ_SCENARIO_SUB_MEMOUT = 5,
	CMDQ_SCENARIO_SUB_ALL = 6,
	CMDQ_SCENARIO_MHL_DISP = 7,
	CMDQ_SCENARIO_RDMA0_DISP = 8,
	CMDQ_SCENARIO_RDMA0_COLOR0_DISP = 9,
	CMDQ_SCENARIO_RDMA1_DISP = 10,

	/* Trigger loop scenario does not enable HWs */
	CMDQ_SCENARIO_TRIGGER_LOOP = 11,

	/* client from user space, so the cmd buffer is in user space. */
	CMDQ_SCENARIO_USER_MDP = 12,

	CMDQ_SCENARIO_DEBUG = 13,
	CMDQ_SCENARIO_DEBUG_PREFETCH = 14,

	/* ESD check */
	CMDQ_SCENARIO_DISP_ESD_CHECK = 15,
	/* for screen capture to wait for RDMA-done without blocking config thread */
	CMDQ_SCENARIO_DISP_SCREEN_CAPTURE = 16,

	/* notifiy there are some tasks exec done in secure path */
	CMDQ_SCENARIO_SECURE_NOTIFY_LOOP = 17,

	CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH = 18,
	CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH = 19,

	/* color path request from kernel */
	CMDQ_SCENARIO_DISP_COLOR = 20,
	/* color path request from user sapce */
	CMDQ_SCENARIO_USER_DISP_COLOR = 21,

	/* [phased out]client from user space, so the cmd buffer is in user space. */
	CMDQ_SCENARIO_USER_SPACE = 22,

	CMDQ_SCENARIO_DISP_MIRROR_MODE = 23,

	CMDQ_SCENARIO_DISP_CONFIG_AAL = 24,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA = 25,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA = 26,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER = 27,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER = 28,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM = 29,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM = 30,
	CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ = 31,
	CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ = 32,
	CMDQ_SCENARIO_DISP_CONFIG_OD = 33,

	CMDQ_SCENARIO_RDMA2_DISP = 34,

	CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP = 35,	/* for primary trigger loop enable pre-fetch usage */
	CMDQ_SCENARIO_LOWP_TRIGGER_LOOP = 36,	/* for low priority monitor loop to polling bus status*/

	CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL = 37,

	CMDQ_SCENARIO_ISP_FDVT = 44,
	CMDQ_SCENARIO_ISP_FDVT_OFF = 46,

	CMDQ_MAX_SCENARIO_COUNT	/* ALWAYS keep at the end */
};

struct ThreadStruct {
	uint32_t taskCount;
	uint32_t waitCookie;
	uint32_t nextCookie;
	struct TaskStruct *pCurTask[CMDQ_MAX_TASK_IN_THREAD_MAX];
};
struct SecIdTbl_t {
	uint32_t port;
	uint32_t sec_id;
};
struct TaskStruct {
	struct list_node listEntry;

	/* For buffer state */
	/* secure driver has to map secure PA to virtual address before access secure DRAM */
	/* so life cycle of pVABase and MVABase are same */
	enum TASK_STATE_ENUM taskState;
	/* VA: denote CMD addr in cmdqSecDr's virtual address space */
	uint32_t *pVABase;
	uint64_t MVABase;		/* IOVA: denote the IOVA for secure CMD */
	uint64_t PABase;		/* PA: denote the PA for secure CMD */
	uint32_t bufferSize;	/* size of allocated command buffer */

	/* For execution */
	int scenario;
	int priority;
	uint64_t engineFlag;
	int commandSize;
	uint32_t *pCMDEnd;
	int thread;		/* pre-dispatch in NWd */
	int irqFlag;	/* flag of IRQ received */
	uint8_t hwid;

	int waitCookie; /* task index in thread's tasklist. dispatched by NWd*/
	bool resetExecCnt;
	uint64_t enginesNeedDAPC;
	uint64_t enginesNeedPortSecurity;
	uint32_t sec_id;
	struct SecIdTbl_t pSecIdTbl[CMDQ_IWC_MAX_ADDR_LIST_LENGTH];
	uint32_t SecIdTblLength;
	/* Debug */
	uint64_t hNormalTask;
	bool	throwAEE;
};

struct ContextStruct {
	/* Basic information */
	struct TaskStruct taskInfo[CMDQ_MAX_FIXED_TASK];
	struct ThreadStruct thread[CMDQ_MAX_THREAD_COUNT]; // note we only handle secure thread

	/* Share region with NWd */
	uint64_t sharedThrExecCntPA;	/* PA start address of THR cookie */
	uint64_t sharedThrExecCntSize;
	bool initPathResDone;

	/* Error information */
	int	errNum;
	int	logLevel;
	int	enableProfile;
	bool bypassIrqNotify;
	//default false. true for disable that IRQ handler thread notify (testcase only)
};

struct cmdq_protect_engine {
	uint64_t engine_flag;
	uint32_t dapc_reg_offset;
	uint32_t bit;
	uint8_t sys;
	uint8_t dapc_level;
};

struct engine_secure_port {
	uint64_t engine_flag;
	uint32_t port;
};

struct cmdq_sec_handle_reg {
	uint32_t addr;
	int32_t engine;
};

struct tlApiCmdqExecMetadata_t {
	uint32_t pSecFdAddr[CMDQ_IWC_MAX_ADDR_LIST_LENGTH];
	uint32_t pSecFdCount;
	uint16_t ovl_handle[CMDQ_IWC_MAX_ADDR_LIST_LENGTH];
	struct SecIdTbl_t pSecIdTbl[CMDQ_IWC_MAX_ADDR_LIST_LENGTH];
	uint32_t SecIdTblLength;

	struct isp_exec_metadata isp_execmeta;
};

/**
 * IPC execution data (between TL* and cmdqSecDr)
 *
 * @paramc pIwcCmdqMessage [IN]  cmdqSecDr IWC message
 * @param execMetadata  [IN]  cmdqSecDr execution metadata about secure buffer address etc
 *
 */
struct DrIPCData_t {
	struct iwcCmdqMessage_t *pIwcCmdqMessage;
	struct iwcCmdqMessageEx_t *message_ex;
	struct iwcCmdqMessageEx2_t *message_ex2;
	struct tlApiCmdqExecMetadata_t execMetadata;
};

uint64_t *cmdq_task_get_va_by_offset(struct TaskStruct *task, uint32_t offset);
uint64_t cmdq_task_get_curr_pa(struct TaskStruct *task);
uint64_t cmdq_task_get_pa_by_offset(struct TaskStruct *task, uint32_t offset);
int cmdq_task_assign_command(struct TaskStruct *task,
	uint16_t reg_idx, uint32_t value);
int cmdq_task_store_value_reg(struct TaskStruct *task, u16 indirect_dst_reg_idx,
	u16 dst_addr_low, u16 indirect_src_reg_idx, u32 mask);
int cmdq_task_write_reg_addr(struct TaskStruct *task, uint64_t addr,
	u16 src_reg_idx, u32 mask);
int cmdq_task_write_indriect(struct TaskStruct *task, struct cmdq_base *clt_base,
	uint64_t addr, u16 src_reg_idx, u32 mask);
int cmdq_task_logic_command(struct TaskStruct *task, enum CMDQ_LOGIC_ENUM s_op,
	u16 result_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand);
int cmdq_task_sleep(struct TaskStruct *task, u32 tick, u16 reg_gpr);
int cmdq_task_poll_timeout(struct TaskStruct *task, u32 value,
	phys_addr_t addr, u32 mask, u16 count, u16 reg_gpr);
int cmdq_task_clear_event(struct TaskStruct *task, uint16_t event);
int cmdq_task_cond_jump(struct TaskStruct *task,
	u16 offset_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand,
	enum CMDQ_CONDITION_ENUM condition_operator);
int cmdq_task_wfe(struct TaskStruct *task, uint16_t event);
int cmdq_task_write_value_addr(struct TaskStruct *task,
	uint64_t addr, uint32_t value, uint32_t mask);
int cmdq_task_read(struct TaskStruct *task,
	uint64_t src_addr, uint16_t dst_reg_idx);
int cmdq_task_set_event(struct TaskStruct *task, uint16_t event);
void cmdq_tz_assign_tzmp_command(struct TaskStruct *pTask);
void cmdq_task_cb(struct TaskStruct *pTask);
void cmdq_set_plat_ops(const struct pkvm_module_ops *ops);
void cmdq_set_isp_ops(const struct pkvm_module_ops *ops);
void cmdq_set_fdvt_ops(const struct pkvm_module_ops *ops);
int32_t cmdq_tz_set_dapc_security_reg(struct TaskStruct *task, bool enable, bool use_cmdq);
int32_t cmdq_tz_set_port_security_reg(struct TaskStruct *pTask, bool enable, bool useCmdq);
void cmdqUtilPrintHexDump(const char *prefix_str, uint32_t *buf,
	uint32_t len, uint64_t pa);
bool is_mdp_thread(const int32_t hwid, const int32_t thrd);
int cmdq_task_finalize_loop(struct TaskStruct *task);
void cmdq_tz_mdp_handle(struct TaskStruct *pTask);
void cmdq_secio_write(const uint32_t base, const uint32_t addr, const uint32_t val);
uint32_t cmdq_secio_read(const uint32_t base, const uint32_t addr);
#ifdef CMDQ_SECIO_WA
TZ_RESULT __SECIO_WRITE(uint32_t io_type, uint32_t reg_offset, uint32_t write_val);
TZ_RESULT __SECIO_READ(uint32_t io_type, uint32_t reg_offset, uint32_t *read_val);
#endif
#endif	/*  __PKVM_CMDQ_HYP_H__ */
