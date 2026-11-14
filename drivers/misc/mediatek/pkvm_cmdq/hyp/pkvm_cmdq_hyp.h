/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <asm/kvm_pkvm_module.h>

#define PFX_CMDQ_MSG "[cmdq] "
#define PFX_CMDQ_ERR "[cmdq][err] "
#include "list.h"

#define CMDQ_MAX_SECURE_CORE_COUNT	(4)
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

#define CMDQ_REG_GET32(addr)        (cmdq_secio_read(addr))
#define CMDQ_REG_SET32(addr, val)   (cmdq_secio_write(addr, val))

/* CMDQ THRx */
#define CMDQ_CURR_IRQ_STATUS         (GCE_BASE_VA + 0x010)
#define CMDQ_SECURE_IRQ_STATUS       (GCE_BASE_VA + 0x014)
#define CMDQ_CURR_LOADED_THR         (GCE_BASE_VA + 0x018)
#define CMDQ_THR_SLOT_CYCLES         (GCE_BASE_VA + 0x030)
#define CMDQ_THR_EXEC_CYCLES         (GCE_BASE_VA + 0x034)
#define CMDQ_THR_TIMEOUT_TIMER       (GCE_BASE_VA + 0x038)
#define CMDQ_BUS_CONTROL_TYPE        (GCE_BASE_VA + 0x040)
#define CMDQ_CURR_INST_ABORT         (GCE_BASE_VA + 0x020)
#define CMDQ_SECURITY_ABORT          (GCE_BASE_VA + 0x050)

#define CMDQ_SECURITY_STA(id)        (GCE_BASE_VA + (0x030 * id) + 0x024)
#define CMDQ_SECURITY_SET(id)        (GCE_BASE_VA + (0x030 * id) + 0x028)
#define CMDQ_SECURITY_CLR(id)        (GCE_BASE_VA + (0x030 * id) + 0x02C)

#define CMDQ_SYNC_TOKEN_ID           (GCE_BASE_VA + 0x060)
#define CMDQ_SYNC_TOKEN_VAL          (GCE_BASE_VA + 0x064)
#define CMDQ_SYNC_TOKEN_UPD          (GCE_BASE_VA + 0x068)

#define CMDQ_GPR_R32(id)             (GCE_BASE_VA + (0x004 * id) + 0x80)

#define CMDQ_THR_WARM_RESET(id)      (GCE_BASE_VA + (0x080 * id) + 0x100)
#define CMDQ_THR_ENABLE_TASK(id)     (GCE_BASE_VA + (0x080 * id) + 0x104)
#define CMDQ_THR_SUSPEND_TASK(id)    (GCE_BASE_VA + (0x080 * id) + 0x108)
#define CMDQ_THR_CURR_STATUS(id)     (GCE_BASE_VA + (0x080 * id) + 0x10C)
#define CMDQ_THR_IRQ_STATUS(id)      (GCE_BASE_VA + (0x080 * id) + 0x110)
#define CMDQ_THR_IRQ_ENABLE(id)      (GCE_BASE_VA + (0x080 * id) + 0x114)
#define CMDQ_THR_CURR_ADDR(id)       (GCE_BASE_VA + (0x080 * id) + 0x120)
#define CMDQ_THR_END_ADDR(id)        (GCE_BASE_VA + (0x080 * id) + 0x124)
#define CMDQ_THR_EXEC_CNT(id)        (GCE_BASE_VA + (0x080 * id) + 0x128)
#define CMDQ_THR_WAIT_TOKEN(id)      (GCE_BASE_VA + (0x080 * id) + 0x130)
#define CMDQ_THR_CFG(id)             (GCE_BASE_VA + (0x080 * id) + 0x140)
#define CMDQ_THR_INST_CYCLES(id)     (GCE_BASE_VA + (0x080 * id) + 0x150)
#define CMDQ_THR_INST_THRESX(id)     (GCE_BASE_VA + (0x080 * id) + 0x154)

#define CMDQ_THR_SPR0(id)	(GCE_BASE_VA + (0x080 * id) + 0x160)
#define CMDQ_THR_SPR1(id)	(GCE_BASE_VA + (0x080 * id) + 0x164)
#define CMDQ_THR_SPR2(id)	(GCE_BASE_VA + (0x080 * id) + 0x168)
#define CMDQ_THR_SPR3(id)	(GCE_BASE_VA + (0x080 * id) + 0x16c)

#define CMDQ_GET_COOKIE_CNT(thd) \
	(CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thd)) & CMDQ_MAX_COOKIE_VALUE)

extern bool mtkcam_security_cam_normal_preview_support;

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
};

enum CMDQ_SEC_ENG_ENUM {
	/* MDP */
	CMDQ_SEC_MDP_RDMA0 = 0,
	CMDQ_SEC_MDP_RDMA1,	/* 1 */
	CMDQ_SEC_MDP_WDMA,	/* 2 */
	CMDQ_SEC_MDP_WROT0,	/* 3 */
	CMDQ_SEC_MDP_WROT1,	/* 4 */

	/* DISP */
	CMDQ_SEC_DISP_RDMA0,	/* 5 */
	CMDQ_SEC_DISP_RDMA1,	/* 6 */
	CMDQ_SEC_DISP_WDMA0,	/* 7 */
	CMDQ_SEC_DISP_WDMA1,	/* 8 */
	CMDQ_SEC_DISP_OVL0,	/* 9 */
	CMDQ_SEC_DISP_OVL1,	/* 10 */
	CMDQ_SEC_DISP_OVL2,	/* 11 */
	CMDQ_SEC_DISP_2L_OVL0,	/* 12 */
	CMDQ_SEC_DISP_2L_OVL1,	/* 13 */
	CMDQ_SEC_DISP_2L_OVL2,	/* 14 */

	/* ISP */
	CMDQ_SEC_ISP_IMGI,	/* 15 */
	CMDQ_SEC_ISP_VIPI,	/* 16 */
	CMDQ_SEC_ISP_LCEI,	/* 17 */
	CMDQ_SEC_ISP_IMG2O,	/* 18 */
	CMDQ_SEC_ISP_IMG3O,	/* 19 */
	CMDQ_SEC_ISP_SMXIO,	/* 20 */
	CMDQ_SEC_ISP_DMGI_DEPI, /* 21 */
	CMDQ_SEC_ISP_IMGCI,	/* 22 */
	CMDQ_SEC_ISP_TIMGO,	/* 23 */
	CMDQ_SEC_DPE,		/* 24 */
	CMDQ_SEC_OWE,		/* 25 */
	CMDQ_SEC_WPEI,		/* 26 */
	CMDQ_SEC_WPEO,		/* 27 */
	CMDQ_SEC_WPEI2,		/* 28 */
	CMDQ_SEC_WPEO2,		/* 29 */
	CMDQ_SEC_FDVT,		/* 30 */
	CMDQ_SEC_ISP_UFBCI,	/* 31 */
	CMDQ_SEC_ISP_UFBCO,	/* 32 */

	CMDQ_SEC_MDP_WROT2,	/* 33 */
	CMDQ_SEC_MDP_WROT3,	/* 34 */
	CMDQ_SEC_MDP_RDMA2,	/* 35 */
	CMDQ_SEC_MDP_RDMA3,	/* 36 */

	CMDQ_SEC_VENC_BSDMA,	    /* 37 */
	CMDQ_SEC_VENC_CUR_LUMA,	    /* 38 */
	CMDQ_SEC_VENC_CUR_CHROMA,	/* 39 */
	CMDQ_SEC_VENC_REF_LUMA,     /* 40 */
	CMDQ_SEC_VENC_REF_CHROMA,	/* 41 */
	CMDQ_SEC_VENC_REC,          /* 42 */
	CMDQ_SEC_VENC_SUB_R_LUMA,   /* 43 */
	CMDQ_SEC_VENC_SUB_W_LUMA,   /* 44 */
	CMDQ_SEC_VENC_SV_COMV,      /* 45 */
	CMDQ_SEC_VENC_RD_COMV,      /* 46 */
	CMDQ_SEC_VENC_NBM_RDMA,     /* 47 */
	CMDQ_SEC_VENC_NBM_WDMA,     /* 48 */
	CMDQ_SEC_VENC_NBM_RDMA_LITE,/* 49 */
	CMDQ_SEC_VENC_NBM_WDMA_LITE,/* 50 */
	CMDQ_SEC_VENC_FCS_NBM_RDMA, /* 51 */
	CMDQ_SEC_VENC_FCS_NBM_WDMA, /* 52 */
	CMDQ_SEC_MDP_HDR0,          /* 53 */
	CMDQ_SEC_MDP_HDR1,          /* 54 */
	CMDQ_SEC_MDP_AAL0,          /* 55 */
	CMDQ_SEC_MDP_AAL1,          /* 56 */
	CMDQ_SEC_MDP_AAL2,          /* 57 */
	CMDQ_SEC_MDP_AAL3,          /* 58 */

	CMDQ_SEC_MAX_ENG_COUNT	/* ALWAYS keep at the end */
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

	/* Debug */
	uint64_t hNormalTask;
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
int32_t cmdq_tz_set_dapc_security_reg(struct TaskStruct *task, bool enable, bool use_cmdq);
