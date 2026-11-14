/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_EB_H__
#define __GED_EB_H__

#include <linux/types.h>
#include "ged_type.h"
#include "ged_dvfs.h"

/**************************************************
 * GPU FAST DVFS Log Setting
 **************************************************/
#define GED_FAST_DVFS_TAG "[GPU/FDVFS]"
#define GPUFDVFS_LOGE(fmt, args...) \
	pr_info(GED_FAST_DVFS_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGW(fmt, args...) \
	pr_debug(GED_FAST_DVFS_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGI(fmt, args...) \
	pr_info(GED_FAST_DVFS_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGD(fmt, args...) \
	pr_debug(GED_FAST_DVFS_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)


/**************************************************
 * GPU FAST DVFS SYSRAM Setting
 **************************************************/
#define FASTDVFS_COUNTER_FIRST_ENTRY 48   // use to offset array
#define FASTDVFS_FEEDBACK_INFO_FIRST_ENTRY 88  // use to offset array

#define SYSRAM_LOG_SIZE sizeof(int)
#define COMMON_LOW_BIT		(0)
#define COMMON_MID_BIT		(8)
#define COMMON_HIGH_BIT		(16)

// FDVFS SYSRAM space is allocated after QOSc & SLC, 48~77
enum gpu_fastdvfs_counter {
	FASTDVFS_GPU_RISKY_BQ_STATE = FASTDVFS_COUNTER_FIRST_ENTRY,
	FASTDVFS_GPU_RISKY_COMPLETE_TIME,
	FASTDVFS_GPU_RISKY_UNCOMPLETE_TIME,
	FASTDVFS_GPU_RISKY_COMPLETE_TARGET_TIME,
	FASTDVFS_GPU_RISKY_UNCOMPLETE_TARGET_TIME,
	FASTDVFS_GPU_RISKY_COMPLETE_COUNT,
	FASTDVFS_GPU_FB_QUEUE_TIMESTAMP,
	FASTDVFS_GPU_FB_DONE_TIMESTAMP,
	FASTDVFS_GPU_FB_TARGET_HD,
	FASTDVFS_GPU_TARGET_FPS,
	FASTDVFS_GPU_EB_DESIRE_FREQ_ID,
	FASTDVFS_GPU_EB_DESIRE_POLICY_STATE,
	FASTDVFS_GPU_EB_DEBUG_READ_POINTER,
	FASTDVFS_GPU_EB_DEBUG_WRITE_POINTER,
	FASTDVFS_GPU_EB_DCS_CORE_NUM,
	FASTDVFS_GPU_EB_VIRTUAL_OPP,
	FASTDVFS_GPU_EB_UNCOMPLETE_COUNT,
	FASTDVFS_GPU_FB_FALLBACK_RESET_COUNT,
	FASTDVFS_COUNTER_LAST_COMMIT_IDX = 66,
	FASTDVFS_COUNTER_LAST_COMMIT_TOP_IDX,
	FASTDVFS_GPU_FB_TARGET_SOC_TIMER_HI,
	FASTDVFS_GPU_FB_TARGET_SOC_TIMER_LO,
	FASTDVFS_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI,
	FASTDVFS_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO,
	FASTDVFS_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_HI,
	FASTDVFS_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_LO,
	FASTDVFS_GPU_EB_USE_FB_SOC_TIMER_HI,
	FASTDVFS_GPU_EB_USE_FB_SOC_TIMER_LO,
	FASTDVFS_GPU_EB_ASYNC_RATIO_ENABLE = 77,
	FASTDVFS_GPU_PWR_HINT = 78,
	FASTDVFS_GPU_EB_LOG_DUMP_TOP_FREQ = 79,
	FASTDVFS_GPU_EB_LOG_DUMP_STACK_FREQ = 89,			// real stack, virtual stack
	FASTDVFS_GPU_EB_LOG_DUMP_LOADING1 = 99,			// GPU,MCU,ITER,AVG
	FASTDVFS_GPU_EB_LOG_DUMP_LOADING2 = 109,			// reserve
	FASTDVFS_GPU_EB_LOG_DUMP_POWER_STATE = 119,
	FASTDVFS_GPU_EB_LOG_DUMP_DEBUG_COUNT = 129,
	FASTDVFS_GPU_EB_LOG_DUMP_SOC_TIMER_HI = 139,
	FASTDVFS_GPU_EB_LOG_DUMP_SOC_TIMER_LO = 149,
	FASTDVFS_GPU_EB_LOG_DUMP_GPU_TIME = 159,
	FASTDVFS_GPU_EB_LOG_DUMP_OPP = 169,			// cur, target
	FASTDVFS_GPU_EB_LOG_DUMP_BOUND = 179,
	FASTDVFS_GPU_EB_LOG_DUMP_MARGIN = 189,
	FASTDVFS_GPU_EB_STABLE_LB = 204,
	FASTDVFS_GPU_EB_P_MODE_STATUS = 205,
	FASTDVFS_GPU_EB_SMALL_FRAME  = 206,
	FASTDVFS_GPU_EB_2K_SIZE = 207,
	FASTDVFS_GPU_EB_26M_REPLACE  = 208,
	FASTDVFS_GPU_EB_LOADING_MODE                = 209,
	FASTDVFS_GPU_EB_API_BOOST = 210,
	FASTDVFS_GPU_EB_ASYNC_PARAM,
	FASTDVFS_GPU_EB_DCS_ENABLE,
	FASTDVFS_GPU_EB_USE_TARGET_GPU_HD,
	FASTDVFS_GPU_EB_USE_ITER_U_MCU_LOADING = 215,
	FASTDVFS_GPU_EB_USE_GPU_LOADING = 216,
	FASTDVFS_GPU_EB_USE_MCU_LOADING = 217,
	FASTDVFS_GPU_EB_USE_ITER_LOADING = 218,
	FASTDVFS_GPU_EB_USE_DELTA_TIME = 219,
	FASTDVFS_GPU_EB_USE_APPLY_LB_ASYNC = 220,
	FASTDVFS_GPU_EB_USE_MAX_IS_MCU,
	FASTDVFS_GPU_EB_USE_AVG_MCU,
	FASTDVFS_GPU_EB_USE_MAX_MCU,
	FASTDVFS_GPU_EB_USE_AVG_MCU_TH,
	FASTDVFS_GPU_EB_USE_MAX_MCU_TH,
	FASTDVFS_GPU_EB_USE_ASYNC_GPU_ACTIVE,
	FASTDVFS_GPU_EB_USE_ASYNC_ITER,
	FASTDVFS_GPU_EB_USE_ASYNC_COMPUTE,
	FASTDVFS_GPU_EB_USE_ASYNC_L2EXT,
	FASTDVFS_GPU_EB_USE_ASYNC_TILER,
	FASTDVFS_GPU_EB_USE_ASYNC_MCU,
	FASTDVFS_GPU_EB_USE_PERF_IMPROVE,
	FASTDVFS_GPU_EB_USE_ADJUST_RATIO,
	FASTDVFS_GPU_EB_USE_ASYNC_OPP_DIFF,
	FASTDVFS_GPU_EB_GED_PRESERVE = 235,
	FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_HI = 236,
	FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_LO = 237,
	FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_OPP = 238,
	FASTDVFS_GPU_EB_USE_POLICY_STATE = 239,
	FASTDVFS_GPU_EB_USE_T_GPU,
	FASTDVFS_GPU_EB_USE_TARGET_OPP,
	FASTDVFS_GPU_EB_USE_TARGET_GPU,
	FASTDVFS_GPU_EB_USE_COMPLETE_GPU,
	FASTDVFS_GPU_EB_USE_UNCOMPOLETE_GPU,
	FASTDVFS_GPU_EB_USE_MARGIN,
	FASTDVFS_GPU_EB_USE_MARGIN_CEIL,
	FASTDVFS_GPU_EB_USE_MARGIN_FLOOR,
	FASTDVFS_GPU_EB_USE_BOUND_LOW,
	FASTDVFS_GPU_EB_USE_BOUND_HIGH,
	FASTDVFS_GPU_EB_USE_BOUND_ULTRA_LOW,
	FASTDVFS_GPU_EB_USE_BOUND_ULTRA_HIGH,
	FASTDVFS_GPU_EB_USE_LOADING,
	FASTDVFS_GPU_EB_USE_FB_OVERDUE_TIME,
	FASTDVFS_GPU_EB_USE_DEBUG_COUNT,
	FASTDVFS_GPU_EB_LOG_DUMP_FB_MONITOR = 256,
	FASTDVFS_GPU_EB_LOG_DUMP_UN_TIME = 266,
	FASTDVFS_GPU_EB_LOG_DUMP_COM_TIME = 276,
	FASTDVFS_GPU_EB_LOG_DUMP_UN_TIME_TARGET = 286,
	FASTDVFS_GPU_EB_LOG_DUMP_COM_TIME_TARGET = 296,
	FASTDVFS_GPU_EB_LOG_DUMP_TIME_TARGET = 306,
	FASTDVFS_GPU_EB_LOG_DUMP_DELTA_TIME = 316,
	FASTDVFS_GPU_EB_LOG_DUMP_FB_TARGET = 326,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_GPU = 336,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_ITER = 346,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_COMPUTE = 356,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_L2EXT = 366,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_TILER = 376,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_MCU = 386,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX1 = 396,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX2 = 406,
	FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX3 = 416,
	FASTDVFS_GPU_EB_CMD_FALLBACK_INTERVAL = 426,
	FASTDVFS_GPU_EB_CMD_FALLBACK_WIN_SIZE = 427,
	FASTDVFS_GPU_EB_CMD_LOADING_BASE_DVFS_STEP = 428,
	FASTDVFS_GPU_EB_CMD_LOADING_STRIDE_SIZE = 429,
	FASTDVFS_GPU_EB_CMD_LOADING_WIN_SIZE = 430,
	FASTDVFS_GPU_EB_CMD_TIMER_BASE_DVFS_MARGIN = 431,
	FASTDVFS_GPU_EB_LOG_DUMP_PREOC = 432,
	FASTDVFS_GPU_EB_USE_IDX_NOTIFY = 442,
	NR_FASTDVFS_COUNTER,
};

/* 6989 0x117800~0x117C00 */
#define FASTDVFS_POWERMODEL_SYSRAM_BASE 0x117800U

#define SYSRAM_GPU_RISKY_BQ_STATE \
( \
(FASTDVFS_GPU_RISKY_BQ_STATE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_COMPLETE_TIME \
( \
(FASTDVFS_GPU_RISKY_COMPLETE_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_UNCOMPLETE_TIME \
( \
(FASTDVFS_GPU_RISKY_UNCOMPLETE_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_COMPLETE_TARGET_TIME \
( \
(FASTDVFS_GPU_RISKY_COMPLETE_TARGET_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_UNCOMPLETE_TARGET_TIME \
( \
(FASTDVFS_GPU_RISKY_UNCOMPLETE_TARGET_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_COMPLETE_COUNT \
( \
(FASTDVFS_GPU_RISKY_COMPLETE_COUNT*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_QUEUE_TIMESTAMP \
( \
(FASTDVFS_GPU_FB_QUEUE_TIMESTAMP*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_DONE_TIMESTAMP \
( \
(FASTDVFS_GPU_FB_DONE_TIMESTAMP*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_TARGET_HD \
( \
(FASTDVFS_GPU_FB_TARGET_HD*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_TARGET_FPS \
( \
(FASTDVFS_GPU_TARGET_FPS*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_DESIRE_FREQ_ID \
( \
(FASTDVFS_GPU_EB_DESIRE_FREQ_ID*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_DESIRE_POLICY_STATE \
( \
(FASTDVFS_GPU_EB_DESIRE_POLICY_STATE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_GPU_EB_DEBUG_READ_POINTER \
( \
(FASTDVFS_GPU_EB_DEBUG_READ_POINTER*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_GPU_EB_DEBUG_WRITE_POINTER \
( \
(FASTDVFS_GPU_EB_DEBUG_WRITE_POINTER*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_DCS_CORE_NUM \
( \
(FASTDVFS_GPU_EB_DCS_CORE_NUM*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_VIRTUAL_OPP \
( \
(FASTDVFS_GPU_EB_VIRTUAL_OPP *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_UNCOMPLETE_COUNT \
( \
(FASTDVFS_GPU_EB_UNCOMPLETE_COUNT *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_FALLBACK_RESET_COUNT \
( \
(FASTDVFS_GPU_FB_FALLBACK_RESET_COUNT *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_LAST_COMMIT_IDX \
( \
(FASTDVFS_COUNTER_LAST_COMMIT_IDX*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_LAST_COMMIT_TOP_IDX \
( \
(FASTDVFS_COUNTER_LAST_COMMIT_TOP_IDX*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_TARGET_SOC_TIMER_HI \
( \
(FASTDVFS_GPU_FB_TARGET_SOC_TIMER_HI*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FB_TARGET_SOC_TIMER_LO \
( \
(FASTDVFS_GPU_FB_TARGET_SOC_TIMER_LO*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI \
( \
(FASTDVFS_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO \
( \
(FASTDVFS_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_HI \
( \
(FASTDVFS_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_HI*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_LO \
( \
(FASTDVFS_GPU_EB_USE_UNCOMPLETE_SOC_TIMER_LO*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_FB_SOC_TIMER_HI \
( \
(FASTDVFS_GPU_EB_USE_FB_SOC_TIMER_HI*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_FB_SOC_TIMER_LO \
( \
(FASTDVFS_GPU_EB_USE_FB_SOC_TIMER_LO*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_ASYNC_RATIO_ENABLE  \
(									   \
(FASTDVFS_GPU_EB_ASYNC_RATIO_ENABLE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_PWR_HINT \
( \
(FASTDVFS_GPU_PWR_HINT*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_TOP_FREQ               \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_TOP_FREQ*SYSRAM_LOG_SIZE)	  \
)
#define SYSRAM_GPU_EB_LOG_DUMP_STACK_FREQ               \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_STACK_FREQ*SYSRAM_LOG_SIZE)   \
)
#define SYSRAM_GPU_EB_LOG_DUMP_LOADING1 \
( \
(FASTDVFS_GPU_EB_LOG_DUMP_LOADING1*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_LOADING2 \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_LOADING2*SYSRAM_LOG_SIZE)   \
)
#define SYSRAM_GPU_EB_LOG_DUMP_POWER_STATE \
( \
(FASTDVFS_GPU_EB_LOG_DUMP_POWER_STATE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_DEBUG_COUNT \
( \
(FASTDVFS_GPU_EB_LOG_DUMP_DEBUG_COUNT*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_SOC_TIMER_HI             \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_SOC_TIMER_HI*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_SOC_TIMER_LO             \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_SOC_TIMER_LO*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_GPU_TIME             \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_GPU_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_OPP \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_OPP*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_BOUND \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_BOUND*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_MARGIN \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_MARGIN*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_FB_MONITOR \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_FB_MONITOR*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_UN_TIME \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_UN_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_COM_TIME \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_COM_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_UN_TIME_TARGET \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_UN_TIME_TARGET*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_COM_TIME_TARGE \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_COM_TIME_TARGET*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_TIME_TARGET \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_TIME_TARGET*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_DELTA_TIME \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_DELTA_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_FB_TARGET \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_FB_TARGET*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_GPU \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_GPU*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_ITER \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_ITER*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_COMPUTE \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_COMPUTE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_L2EXT \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_L2EXT*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_TILER \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_TILER*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_MCU \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_MCU*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX1 \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX1*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX2 \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX2*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX3 \
(\
(FASTDVFS_GPU_EB_LOG_DUMP_ASYNC_INDEX3*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_2K_SIZE \
(\
(FASTDVFS_GPU_EB_2K_SIZE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_ITER_U_MCU_LOADING					\
(\
(FASTDVFS_GPU_EB_USE_ITER_U_MCU_LOADING*SYSRAM_LOG_SIZE)		\
)
#define SYSRAM_GPU_EB_USE_GPU_LOADING					\
(\
(FASTDVFS_GPU_EB_USE_GPU_LOADING*SYSRAM_LOG_SIZE)		\
)
#define SYSRAM_GPU_EB_USE_MCU_LOADING					\
(\
(FASTDVFS_GPU_EB_USE_MCU_LOADING*SYSRAM_LOG_SIZE)		\
)
#define SYSRAM_GPU_EB_USE_ITER_LOADING					\
(\
(FASTDVFS_GPU_EB_USE_ITER_LOADING*SYSRAM_LOG_SIZE)		\
)
#define SYSRAM_GPU_EB_USE_DELTA_TIME					\
(\
(FASTDVFS_GPU_EB_USE_DELTA_TIME*SYSRAM_LOG_SIZE)		\
)
#define SYSRAM_GPU_EB_GED_KERNEL_COMMIT_OPP              \
(\
(FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_OPP *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_LO                \
(\
(FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_LO*SYSRAM_LOG_SIZE)    \
)
#define SYSRAM_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_HI              \
(\
(FASTDVFS_GPU_EB_GED_KERNEL_COMMIT_SOC_TIMER_HI*SYSRAM_LOG_SIZE)  \
)
#define SYSRAM_GPU_EB_GED_PRESERVE              \
(\
(FASTDVFS_GPU_EB_GED_PRESERVE*SYSRAM_LOG_SIZE)  \
)
#define SYSRAM_GPU_EB_LOADING_MODE             \
(													   \
(FASTDVFS_GPU_EB_LOADING_MODE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_P_MODE_STATUS             \
(													   \
(FASTDVFS_GPU_EB_P_MODE_STATUS*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_26M_REPLACE             \
(													   \
(FASTDVFS_GPU_EB_26M_REPLACE*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_API_BOOST                        \
(													   \
(FASTDVFS_GPU_EB_API_BOOST*SYSRAM_LOG_SIZE)            \
)
#define SYSRAM_GPU_EB_ASYNC_PARAM                      \
(													   \
(FASTDVFS_GPU_EB_ASYNC_PARAM*SYSRAM_LOG_SIZE)          \
)
#define SYSRAM_GPU_EB_DCS_ENABLE                      \
(													   \
(FASTDVFS_GPU_EB_DCS_ENABLE*SYSRAM_LOG_SIZE)          \
)
#define SYSRAM_GPU_EB_USE_TARGET_GPU_HD				\
(\
(FASTDVFS_GPU_EB_USE_TARGET_GPU_HD*SYSRAM_LOG_SIZE)		  \
)
#define SYSRAM_GPU_EB_USE_POLICY_STATE              \
(\
(FASTDVFS_GPU_EB_USE_POLICY_STATE *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_T_GPU \
( \
(FASTDVFS_GPU_EB_USE_T_GPU *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_TARGET_OPP \
( \
(FASTDVFS_GPU_EB_USE_TARGET_OPP *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_TARGET_GPU \
( \
(FASTDVFS_GPU_EB_USE_TARGET_GPU *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_COMPLETE_GPU \
( \
(FASTDVFS_GPU_EB_USE_COMPLETE_GPU *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_UNCOMPOLETE_GPU \
( \
(FASTDVFS_GPU_EB_USE_UNCOMPOLETE_GPU *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_MARGIN \
( \
(FASTDVFS_GPU_EB_USE_MARGIN *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_MARGIN_CEIL \
( \
(FASTDVFS_GPU_EB_USE_MARGIN_CEIL *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_MARGIN_FLOOR \
( \
(FASTDVFS_GPU_EB_USE_MARGIN_FLOOR *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_BOUND_LOW \
( \
(FASTDVFS_GPU_EB_USE_BOUND_LOW *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_BOUND_HIGH \
( \
(FASTDVFS_GPU_EB_USE_BOUND_HIGH *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_BOUND_ULTRA_LOW \
( \
(FASTDVFS_GPU_EB_USE_BOUND_ULTRA_LOW *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_BOUND_ULTRA_HIGH \
( \
(FASTDVFS_GPU_EB_USE_BOUND_ULTRA_HIGH *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_LOADING \
( \
(FASTDVFS_GPU_EB_USE_LOADING *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_FB_OVERDUE_TIME \
( \
(FASTDVFS_GPU_EB_USE_FB_OVERDUE_TIME *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_DEBUG_COUNT \
( \
(FASTDVFS_GPU_EB_USE_DEBUG_COUNT *SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_USE_APPLY_LB_ASYNC                \
(														\
(FASTDVFS_GPU_EB_USE_APPLY_LB_ASYNC *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_USE_MAX_IS_MCU                    \
(														\
(FASTDVFS_GPU_EB_USE_MAX_IS_MCU *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_AVG_MCU                       \
(														\
(FASTDVFS_GPU_EB_USE_AVG_MCU *SYSRAM_LOG_SIZE)	        \
)
#define SYSRAM_GPU_EB_USE_MAX_MCU                       \
(														\
(FASTDVFS_GPU_EB_USE_MAX_MCU *SYSRAM_LOG_SIZE)	        \
)
#define SYSRAM_GPU_EB_USE_AVG_MCU_TH                    \
(														\
(FASTDVFS_GPU_EB_USE_AVG_MCU_TH *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_MAX_MCU_TH                    \
(														\
(FASTDVFS_GPU_EB_USE_MAX_MCU_TH *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ASYNC_GPU_ACTIVE              \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_GPU_ACTIVE *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_USE_ASYNC_ITER                 \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_ITER *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ASYNC_COMPUTE                 \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_COMPUTE *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_USE_ASYNC_L2EXT                   \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_L2EXT *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ASYNC_TILER                    \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_TILER *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ASYNC_MCU                     \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_MCU *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_PERF_IMPROVE                  \
(														\
(FASTDVFS_GPU_EB_USE_PERF_IMPROVE *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ADJUST_RATIO                  \
(														\
(FASTDVFS_GPU_EB_USE_ADJUST_RATIO *SYSRAM_LOG_SIZE)	    \
)
#define SYSRAM_GPU_EB_USE_ASYNC_OPP_DIFF                \
(														\
(FASTDVFS_GPU_EB_USE_ASYNC_OPP_DIFF *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_SMALL_FRAME                \
(														\
(FASTDVFS_GPU_EB_SMALL_FRAME *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_STABLE_LB                \
(														\
(FASTDVFS_GPU_EB_STABLE_LB *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_FALLBACK_INTERVAL                \
(														\
(FASTDVFS_GPU_EB_CMD_FALLBACK_INTERVAL *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_FALLBACK_WIN_SIZE                \
(														\
(FASTDVFS_GPU_EB_CMD_FALLBACK_WIN_SIZE *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_LB_DVFS_STEP                \
(														\
(FASTDVFS_GPU_EB_CMD_LOADING_BASE_DVFS_STEP *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_LOADING_STRIDE_SIZE                \
(														\
(FASTDVFS_GPU_EB_CMD_LOADING_STRIDE_SIZE *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_LOADING_WIN_SIZE                \
(														\
(FASTDVFS_GPU_EB_CMD_LOADING_WIN_SIZE *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_CMD_TB_DVFS_MARGIN                \
(														\
(FASTDVFS_GPU_EB_CMD_TIMER_BASE_DVFS_MARGIN *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_LOG_DUMP_PREOC                \
(														\
(FASTDVFS_GPU_EB_LOG_DUMP_PREOC *SYSRAM_LOG_SIZE)	\
)
#define SYSRAM_GPU_EB_USE_IDX_NOTIFY                \
(														\
(FASTDVFS_GPU_EB_USE_IDX_NOTIFY *SYSRAM_LOG_SIZE)	\
)


/**********************************************/
enum action_map {
	ACTION_MAP_FASTDVFS = 0,
	ACTION_MAP_FULLTRACE = 1,

	NR_ACTION_MAP
};

/**************************************************
 * GPU FAST DVFS IPI CMD
 **************************************************/

#define FASTDVFS_IPI_TIMEOUT 2000 //ms
#define FDVFS_REDUCE_IPI 1

enum {
	GPUFDVFS_IPI_SET_FRAME_DONE         = 1,
	GPUFDVFS_IPI_GET_BOUND              = 2,
	GPUFDVFS_IPI_SET_NEW_FREQ           = 3,
	GPUFDVFS_IPI_GET_MARGIM             = 4,
	GPUFDVFS_IPI_PMU_START              = 5,
	GPUFDVFS_IPI_SET_FRAME_BASE_DVFS    = 6,
	GPUFDVFS_IPI_SET_TARGET_FRAME_TIME  = 7,
	GPUFDVFS_IPI_SET_FEEDBACK_INFO      = 8,
	GPUFDVFS_IPI_SET_MODE               = 9,
	GPUFDVFS_IPI_GET_MODE               = 10,
	GPUFDVFS_IPI_SET_GED_READY          = 11,
	GPUFDVFS_IPI_SET_POWER_STATE        = 12,
	GPUFDVFS_IPI_SET_DVFS_STRESS_TEST   = 13,
	GPUFDVFS_IPI_SET_DVFS_REINIT        = 14,
	GPUFDVFS_IPI_GET_FB_TUNE_PARAM      = 15,
	GPUFDVFS_IPI_GET_LB_TUNE_PARAM      = 16,
	GPUFDVFS_IPI_SET_KPI_TS             = 17,
	GPUFDVFS_IPI_GET_DEFAULT_POLICY_MODE = 18,
	GPUFDVFS_IPI_GET_KPI_DATA           = 19,
	GPUFDVFS_IPI_GET_TABLE_DATA         = 20,
	GPUFDVFS_IPI_SET_FB_RSF_POLICY      = 21,
	GPUFDVFS_IPI_SET_CONFIG             = 22,
	GPUFDVFS_IPI_SET_FB_MFRC_POLICY     = 23,
	GPUFDVFS_IPI_GET_LOADING_MODE    	= 24,

	NR_GPUFDVFS_IPI,
};

enum {
	GPUFDVFS_KPI_GET_NUM = 1,
	GPUFDVFS_KPI_GET_HEAD = 2,
	GPUFDVFS_KPI_GET_KPI_FROM_HEAD = 3,
	GPUFDVFS_KPI_GET_HEAD_FPS = 4,
	NR_GPUFDVFS_KPI_NUM,
};

enum {
	GPUFDVFS_TABLE_GET_NUM = 1,
	GPUFDVFS_TABLE_GET_COL_1 = 2,
	GPUFDVFS_TABLE_GET_COL_2 = 3,
	NR_GPUFDVFS_TABLE_NUM,
};

enum ged_eb_config_cmd {
	GPUFDVFS_IPI_SET_DTS_INIT_DCS = 1,
	GPUFDVFS_IPI_SET_DCS_STRESS = 2,
	GPUFDVFS_IPI_SET_GOV_ENABLE = 3,
	GPUFDVFS_IPI_SET_GPU_FPS_ENABLE = 4,
	GPUFDVFS_IPI_SET_USE_DEFAULT_MAGIN_ENABLE = 5,
	GPUFDVFS_IPI_SET_MAJOR_MIN_CORE = 6,
	GPUFDVFS_IPI_SET_LOADING_SELECT,
	GPUFDVFS_IPI_GET_LOADING_SELECT,
	GPUFDVFS_IPI_SET_DUMMY_SWITCH,
	GPUFDVFS_IPI_GET_DUMMY_SWITCH,
	GPUFDVFS_IPI_SET_MAX_CONFIG_INDEX,

};

/* IPI data structure */
struct fdvfs_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[5];
		} set_para;
	} u;
};


struct fdvfs_ipi_rcv_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[5];
		} set_para;
	} u;
};

/**************************************************
 * GPU FAST DVFS EVENT IPI
 **************************************************/
enum {
	GPUFDVFS_IPI_EVENT_CLK_CHANGE = 1,
	GPUFDVFS_IPI_EVENT_DEBUG_MODE_ON = 2,
	GPUFDVFS_IPI_EVENT_DEBUG_DATA = 3,
	GPUFDVFS_IPI_EVENT_IDX_CHANGE = 4,
	GPUFDVFS_IPI_EVENT_UPDATE_DESIRE_FREQ = 5,

	NR_GPUFDVFS_IPI_EVENT_CMD,
};

struct GED_EB_EVENT {
	int cmd;
	unsigned int freq_new;
	unsigned int idx[3];
	struct work_struct sWork;
	bool bUsed;
};

struct fastdvfs_event_data {
	unsigned int cmd;
	union {
		struct {
		unsigned int arg[3];
		} set_para;
	} u;
};

/**************************************************
 * GPU KPI TIMESTAMP
 * DequeueBuffer/QueueBuffer/Prefence/GPU_done/set_target_fps
 * Align GED_TIMESTAMP_TYPE_XX
 **************************************************/
enum GPU_KPI_TS {
	GPU_KPI_TS_DEQ = 0x1,
	GPU_KPI_TS_QUE = 0x2,
	GPU_KPI_TS_PRE = 0x10,
	GPU_KPI_TS_DONE = 0x4,
	GPU_KPI_TS_FPS = 0x40,

	GPU_KPI_TS_NUM,
};

/**************************************************
 * Definition
 **************************************************/
#define FDVFS_IPI_DATA_LEN \
	DIV_ROUND_UP(sizeof(struct fdvfs_ipi_data), MBOX_SLOT_SIZE)

#define POLICY_DISABLE      (0)
#define STRESS_TEST         (1)
#define POLICY_MODE         (2)
#define POLICY_DEBUG_FTRACE (4)
#define POLICY_DEBUG_MET    (8)
#define POLICY_MODE_V2      (64)

extern void fdvfs_init(void);
extern void fdvfs_exit(void);
extern int ged_to_fdvfs_command(unsigned int cmd,
	struct fdvfs_ipi_data *fdvfs_d);
extern int mtk_gpueb_sysram_read(int offset);
extern u64 mtk_gpueb_sysram_read_u64(int offset);
extern int mtk_gpueb_sysram_write(int offset, int value);

/**************************************************
 * GPU FAST DVFS EXPORTED API
 **************************************************/
extern int mtk_gpueb_dvfs_set_frame_done(void);
extern unsigned int mtk_gpueb_dvfs_get_cur_freq(void);
extern unsigned int mtk_gpueb_dvfs_get_frame_loading(void);
extern void mtk_gpueb_dvfs_commit(unsigned long ui32NewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited);
extern void mtk_gpueb_dvfs_dcs_commit(unsigned int platform_freq_idx,
		GED_DVFS_COMMIT_TYPE eCommitType, unsigned int virtual_freq_in_MHz);
extern unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable);
extern int mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time,
		unsigned int target_margin);
extern unsigned int
	mtk_gpueb_dvfs_set_feedback_info(int frag_done_interval_in_ns,
	struct GpuUtilization_Ex util_ex, unsigned int curr_fps);
extern unsigned int mtk_gpueb_dvfs_set_mode(unsigned int action);
extern void mtk_gpueb_dvfs_get_mode(struct fdvfs_ipi_data *ipi_data);

unsigned int mtk_gpueb_set_fallback_mode(int fallback_status);
unsigned int mtk_gpueb_set_stability_mode(int stability_status);
void mtk_gpueb_dvfs_get_desire_freq(unsigned long *ui32NewFreqID);
void mtk_gpueb_dvfs_get_desire_freq_dual(unsigned long *stackNewFreqID,
	unsigned long *topNewFreqID);
extern unsigned int is_fdvfs_enable(void);
extern int mtk_gpueb_power_modle_cmd(unsigned int enable);
extern void mtk_swpm_gpu_pm_start(void);
extern int mtk_set_ged_ready(int ged_ready_flag);
/* DVFS IPI */
void mtk_gpueb_set_power_state(enum ged_gpu_power_state power_state);
u64 mtk_gpueb_read_soc_timer(void);
void mtk_gpueb_record_soc_timer(u64 soc_timer);


extern int fastdvfs_proc_init(void);
extern void fastdvfs_proc_exit(void);

#define GPUEB_SYSRAM_DVFS_DEBUG_BUF_READ_OFF 700
#define GPUEB_SYSRAM_GPU_DBG_TEST_BUF_OFF    (GPUEB_SYSRAM_DVFS_DEBUG_BUF_READ_OFF + sizeof(unsigned int))
#define GPUEB_SYSRAM_GPU_DBG_TEST_BUF_SZ     256
#define GPUEB_SYSRAM_DVFS_DEBUG_COUNT        8
#define GPUEB_SYSRAM_DVFS_DEBUG_BUF_SIZE     10

enum ged_eb_dvfs_debug_index {
	EB_COMMIT_TYPE,
	EB_FREQ,
	EB_LOADING,
	EB_AVG_LOADING,
	EB_POWER_STATE,
	EB_DEBUG_COUNT,
	EB_GPU_TIME,
	EB_OPP,
	EB_BOUND,
	EB_MARGIN,
	EB_FB_MONITOR,
	EB_PRESERV,
	EB_ASYNC_COUNTER,
	EB_ASYNC_MCU_INDEX,
	EB_ASYNC_POLICY,
	EB_PREOC,
	EB_MAX_COUNT,
};

enum ged_eb_dvfs_task_index {
	EB_UPDATE_UNCOMPLETE_COUNT,
	EB_UPDATE_POLICY_STATE,
	EB_COMMIT_DCS,
	EB_UPDATE_GPU_TIME_INFO,
	EB_UPDATE_UNCOMPLETE_GPU_TIME,
	EB_UPDATE_FB_TARGET_TIME,
	EB_UPDATE_FB_TARGET_TIME_DONE,
	EB_SET_FTRACE,
	EB_COMMIT_LAST_KERNEL_OPP,
	EB_UPDATE_PRESERVE,
	EB_DCS_ENABLE,
	EB_DCS_CORE_NUM,
	EB_ASYNC_RATIO_ENABLE,
	EB_ASYNC_PARAM,
	EB_UPDATE_API_BOOST,
	EB_REINIT,
	EB_UPDATE_SMALL_FRAME,
	EB_UPDATE_STABLE_LB,
	EB_UPDATE_DESIRE_FREQ_ID,
	EB_UPDATE_LAST_COMMIT_IDX,
	EB_UPDATE_LAST_COMMIT_TOP_IDX,
	EB_SET_PANEL_REFRESH_RATE,
	EB_DBG_CMD,
	EB_FB_RSF_POLICY_ENABLE,
	EB_FB_MFRC_POLICY_ENABLE,
	EB_MAX_INDEX,
};

enum ged_eb_soc_udpate_point {
	SOC_RESET,
	SOC_DEQUEUE,
	SOC_QUEUE,
	SOC_DONE,
	SOC_FB,
	SOC_PREFENCE,
	SOC_MAX_NUM,
};


int ged_eb_dvfs_task(enum ged_eb_dvfs_task_index index, int flag);
void ged_notify_eb_ged_ready(void);

static struct {
	const char *name;
	enum ged_eb_dvfs_debug_index dbg_index;
	unsigned int sysram_base;
} gpueb_dbg_data[] = {
	{"commit_type", EB_COMMIT_TYPE, SYSRAM_GPU_EB_LOG_DUMP_OPP},
	{"freq", EB_FREQ, SYSRAM_GPU_EB_LOG_DUMP_TOP_FREQ},
	{"loading", EB_LOADING, SYSRAM_GPU_EB_LOG_DUMP_LOADING1},
	{"loading_avg", EB_AVG_LOADING, SYSRAM_GPU_EB_LOG_DUMP_LOADING1},
	{"power_state", EB_POWER_STATE, SYSRAM_GPU_EB_LOG_DUMP_POWER_STATE},
	{"debug_count", EB_DEBUG_COUNT, SYSRAM_GPU_EB_LOG_DUMP_DEBUG_COUNT},
	{"gpu_time", EB_GPU_TIME, SYSRAM_GPU_EB_LOG_DUMP_OPP},
	{"opp", EB_OPP, SYSRAM_GPU_EB_LOG_DUMP_OPP},
	{"bound", EB_BOUND, SYSRAM_GPU_EB_LOG_DUMP_BOUND},
	{"margin", EB_MARGIN, SYSRAM_GPU_EB_LOG_DUMP_MARGIN},
	{"fb_monitor", EB_FB_MONITOR, SYSRAM_GPU_EB_LOG_DUMP_FB_MONITOR},
	{"preserv", EB_PRESERV, SYSRAM_GPU_EB_LOG_DUMP_POWER_STATE},
	{"async_counter", EB_ASYNC_COUNTER, SYSRAM_GPU_EB_LOG_DUMP_ASYNC_GPU},
	{"async_mcu_index", EB_ASYNC_MCU_INDEX, SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX1},
	{"async_policy", EB_ASYNC_POLICY, SYSRAM_GPU_EB_LOG_DUMP_ASYNC_INDEX2},
	{"preoc", EB_PREOC, SYSRAM_GPU_EB_LOG_DUMP_PREOC},

};

/******************************************************************
 * SYSRAM for EB_DVFS_V2
 * Order: legacy data => Debug RB => TS RB => Mbrain => Normal data
 * Use virtual offset in ged_eb and calculate real offset in ged_platform
 * virtual offset: DATA_START + index
 *
 * Virtual Debug RB: START ~ START+1024, Each ringbuffer size: 10
 * Virtual TS RB: START+1024 ~ START+2048, Each ringbuffer size: 7
 * Virtual Mbrain: START+2048 ~ START+3072, Each opp size: 4
 * Virtual Normal data: START+3072 ~ END
 ******************************************************************/
#define AP_FDVFS_DATA_START_OFFSET (0x800)
#define AP_FDVFS_TMP_NEGATIVE_OFFSET AP_FDVFS_DATA_START_OFFSET
#define FDVFS_V2_RB_MAX_NUM (0x400)  // 1024
#define FDVFS_TS_DATA_MAX_NUM (0x800)  // 2048
#define FDVFS_MBRAIN_DATA_MAX_NUM (0xC00)  // 3072
#define FDVFS_TS_VIRTUAL_DATA_START (AP_FDVFS_DATA_START_OFFSET + FDVFS_V2_RB_MAX_NUM)
#define FDVFS_MBRAIN_VIRTUAL_DATA_START (AP_FDVFS_DATA_START_OFFSET + FDVFS_TS_DATA_MAX_NUM)
#define FDVFS_NORMAL_VIRTUAL_DATA_START (AP_FDVFS_DATA_START_OFFSET + FDVFS_MBRAIN_DATA_MAX_NUM)

#define MBRAIN_MAX_LOG_SIZE 4 // active: u64, idle: u64
#define RB_TS_COUNT 7
#define RB_LOG_COUNT 10


typedef struct {
	unsigned int type;
	unsigned int lo_ts;
	unsigned int hi_ts;
	unsigned int lo_bqid;
	unsigned int hi_bqid;
	unsigned int pid;
	unsigned int frameid;
	unsigned int isSF;
} GPU_TS_INFO;

// RB for debug
#define GPU_FDVFS_V2_RB_LOG_LIST \
 GEN("Policy__Common", GPU_EB_LOG_DUMP_POLICY_COMMON, 2, "policy_state | eb_commit_type") \
 GEN("Policy__Common__Commit_Reason", GPU_EB_LOG_DUMP_COMMIT_REASON1, 3, "same | diff | is_offscreen") \
 GEN("Policy__Common__Commit_Reason_TID", GPU_EB_LOG_DUMP_COMMIT_REASON2, 3, "count | bqid | pid") \
 GEN("Policy__Loading_based__GPU_Time", GPU_EB_LOG_DUMP_LB_GPU_TIME, 1, "target_hd") \
 GEN("Policy__Loading_based__GPU_Time2", GPU_EB_LOG_DUMP_GPU_TIME_CHECK_TARGET1, 2, "pid | bqid") \
 GEN("Policy__Loading_based__GPU_Time2", GPU_EB_LOG_DUMP_GPU_TIME_CHECK_TARGET2, 2, "fps | use") \
 GEN("Policy__Loading_based__GPU_Time2", GPU_EB_LOG_DUMP_GPU_TIME_CHECK_TARGET3, 1, "t_gpu_target") \
 GEN("Policy__DCS", GPU_EB_LOG_DUMP_DCS1, 4, "gov_support | gov_enable | max_core | current_core") \
 GEN("Policy__DCS", GPU_EB_LOG_DUMP_DCS2, 4, "rdy_current_core | target | fix_core | mode") \
GEN("Policy__DCS__Detail", GPU_EB_LOG_DUMP_DCS_DETAIL1, 1, "core_mask") \
GEN("Policy__DCS__Detail", GPU_EB_LOG_DUMP_DCS_DETAIL2, 1, "rdy_core_mask") \
GEN("Policy__GOV_Detail", GPU_EB_LOG_DUMP_GOV_DETAIL1, 1, "mcu_core_mask") \
 GEN("Policy__GOV_Detail", GPU_EB_LOG_DUMP_GOV_DETAIL2, 1, "gov_mask") \
GEN("GPU_DVFS__ONE_ARG_PRESERVE", GPU_EB_LOG_DUMP_PRESERVE1, 1, "dbg1_1") \
GEN("GPU_DVFS__TWO_ARG_PRESERVE", GPU_EB_LOG_DUMP_PRESERVE2, 2, "dbg2_1 | dbg2_2") \
GEN("GPU_DVFS__THREE_ARG_PRESERVE", GPU_EB_LOG_DUMP_PRESERVE3, 3, "dbg3_1 | dbg3_2 | dbg3_3") \
GEN("GPU_DVFS__FOUR_ARG_PRESERVE", GPU_EB_LOG_DUMP_PRESERVE4, 4, "dbg4_1 | dbg4_2 | dbg4_3 | dbg4_4") \
GEN("EBRB_FREQ2", GPU_EB_LOG_DUMP_STACK_FREQ2, 2, "d_top | d_avg_stack") \
GEN("Policy__Mask_Control", GPU_EB_LOG_DUMP_MASK_CONTROL1, 2, "ipi_cnt | write_gov_cnt") \
GEN("Policy__Mask_Control", GPU_EB_LOG_DUMP_MASK_CONTROL2, 1, "ipi_expected_mask") \
GEN("Policy__Mask_Control", GPU_EB_LOG_DUMP_MASK_CONTROL3, 1, "reg_expected_mask") \
GEN("Policy__PREUVLO1", GPU_EB_LOG_DUMP_PREUVLO1, 2, "count | throttle_freq") \
GEN("Policy__PREUVLO2", GPU_EB_LOG_DUMP_PREUVLO2, 1, "total_time_diff") \
GEN("Policy__DEBUG", GPU_EB_LOG_DUMP_PRESERVE5, 2, "dbg5_1 | dbg5_2")

// sysram
#define GPU_FDVFS_V2_COUNTER_LIST \
 GEN("Panel_refresh_rate", GPU_TS_RB_IDX, 1, "write_idx") \
 GEN("Panel_refresh_rate", GPU_PANEL_REFRESH_RATE, 1, "panel_fps") \
 GEN("Gpu_rb_read_idx", GPU_RB_READ_IDX, 1, "read_idx") \
 GEN("Gpu_rb_full_hint", GPU_RB_FULL_HINT, 1, "rb_full") \
 GEN("Gpu_target_fps_bq", GPU_TARGET_FPS_BQ_LO, 1, "bq_low") \
 GEN("Gpu_target_fps_bq", GPU_TARGET_FPS_BQ_HI, 1, "bq_high") \
 GEN("Gpu_kpi_fps_freq", GPU_KPI_FPS_FREQ, 2, "kpi_fps_freq") \
 GEN("Gpu_kpi_cpu_time", GPU_KPI_CPU_TIME, 1, "kpi_cpu_time") \
 GEN("Gpu_kpi_gpu_time", GPU_KPI_GPU_TIME, 1, "kpi_gpu_time") \
 GEN("Gpu_normalize_loading", GPU_LOADING, 2, "gpu_loading-cnt") \
 GEN("MCU_ITER_normalize_loading", MCU_ITER_LOADING, 2, "MCU_ITER_LOADING") \
 GEN("MCU_ITER_UNION_FRAG_normalize_loading", MCU_ITER_UNION_FRAG_LOADING, 2, "MCU_ITER_UNION_FRAG_LOADING") \
 GEN("COMP_TILE_normalize_loading", COMP_TILE_LOADING, 2, "COMP_TILE_LOADING") \
 GEN("mbrain_sum_loading1", GPU_SUM_LOADING1, 1, "g_sum_loading") \
 GEN("mbrain_sum_loading2", GPU_SUM_LOADING2, 1, "g_sum_loading")\
 GEN("mbrain_sum_time1", GPU_SUM_TIME1, 1, "g_sum_delta_time") \
 GEN("mbrain_sum_time2", GPU_SUM_TIME2, 1, "g_sum_delta_time") \
 GEN("mbrain_opp_cost_ts1", GPU_OPP_COST_TS1, 1, "g_last_opp_cost_update_ts_ms") \
 GEN("mbrain_opp_cost_ts2", GPU_OPP_COST_TS2, 1, "g_last_opp_cost_update_ts_ms") \
 GEN("Gpu_debug_5566", GPU_DEBUG1, 1, "5566_debug1") \
GEN("Gpu_debug_5566", GPU_DEBUG2, 2, "5566_debug2") \
GEN("Gpu_debug_5566", GPU_DEBUG3, 3, "5566_debug3") \
GEN("Gpu_debug_5566", GPU_DEBUG4, 4, "5566_debug4") \
 GEN("Gpu_debug_5566", GPU_DEBUG5, 1, "5566_debug5") \
 GEN("Gpu_debug_5566", GPU_DEBUG6, 1, "5566_debug6") \
 GEN("Gpu_debug_5566", GPU_DEBUG7, 1, "5566_debug7") \
 GEN("Gpu_debug_5566", GPU_DEBUG8, 1, "5566_debug8") \
GEN("enter_set_freq_back", GPU_EB_IS_ENTER_SET_FREQ_BACK, 1, "enter_set_freq_back") \
GEN("policy_state_v2", GPU_EB_USE_POLICY_STATE_V2, 1, "policy_state_v2")\
 GEN("t_gpu", GPU_T_GPU, 1, "t_gpu") \
 GEN("t_gpu_target", GPU_T_GPU_TARGET, 1, "t_gpu_target") \
 GEN("t_gpu_target_hd", GPU_T_GPU_TARGET_HD, 1, "t_gpu_target_hd") \
 GEN("t_q", GPU_T_GPU_FPS_Q, 1, "t_q") \
 GEN("t_pid", GPU_T_GPU_FPS_PID, 1, "t_pid") \
 GEN("t_use", GPU_T_GPU_FPS_USE, 1, "t_use") \
 GEN("t_fps", GPU_T_GPU_FPS, 1, "t_fps") \
 GEN("t_fps_target", GPU_T_GPU_FPS_TARGET, 1, "t_fps_target") \
 GEN("t_gpu_pipe", GPU_T_GPU_PIPE, 1, "t_gpu_pipe") \
 GEN("t_gpu_real", GPU_T_GPU_REAL, 1, "t_gpu_real") \
 GEN("workload_pipe", GPU_WORKLOAD_PIPE, 1, "workload_pipe") \
 GEN("workload_real", GPU_WORKLOAD_REAL, 1, "workload_real") \
 GEN("target_freq", GPU_TARGET_FREQ, 1, "target_freq") \
 GEN("target_freq_restrict", GPU_TARGET_FREQ_RESTRICT, 1, "target_freq_restrict") \
 GEN("target_opp", GPU_TARGET_OPP, 1, "target_opp") \
 GEN("fb_margin", GPU_FB_MARGIN, 1, "fb_margin") \
 GEN("fb_margin_param", GPU_FB_MARGIN_PARAM, 2, "fb_margin_param") \
 GEN("fb_freq_floor", GPU_FB_FREQ_FLOOR, 1, "fb_freq_floor") \
 GEN("fb_busy_cycle_cur", GPU_FB_BUSY_CYCLE_CUR, 1, "fb_busy_cycle_cur") \
 GEN("fb_busy_cycle", GPU_FB_BUSY_CYCLE, 1, "fb_busy_cycle") \
 GEN("t_gpu_target_us", GPU_T_TARGET_US, 1, "t_gpu_target_us") \
 GEN("dcs_gov_core_num", DCS_GOV_CORE_NUM, 1, "dcs_gov_core_num") \
 GEN("dcs_gov_core_mask", DCS_GOV_CORE_MASK, 1, "dcs_gov_core_mask") \
 GEN("major_min_core|major_option", DCS_MAJOR_MIN, 2, "major_min_core|major_option") \
 GEN("g_lowpwr_mode", GPU_LOWPWR_ENABLE, 1, "g_lowpwr_mode") \
 GEN("silence", GPU_LOWPWR_TRACE, 1, "silence") \
 GEN("g_debug", GPU_DEBUG, 1, "g_debug") \
 GEN("fb_mfrc", GPU_FB_MFRC, 1, "fb_mfrc") \
 GEN("is_offscreen", GPU_IS_OFFSCREEN, 1, "is_offscreen") \
 GEN("fb_async_param1", GPU_FB_ASYNC_PARAM1, 4, "fb_async_ratio_param1") \
 GEN("fb_async_param2", GPU_FB_ASYNC_PARAM2, 1, "fb_async_ratio_param2") \
 GEN("fb_npu_hint_ms", GPU_FB_NPU_HINT_MS, 1, "fb_npu_hint_ms") \
 GEN("workload_mode", GPU_EB_WORKLOAD_MODE, 1, "workload_mode") \
GEN("fix_freq_id", GPU_FIX_FREQ_ID, 2, "enable|id") \
GEN("gpu_version", GPU_EB_VERSION, 1, "gpu_version") \
GEN("desire_mask", GOV_DESIRE_MASK, 1, "desire_mask") \
GEN("fb_mfrc", GPU_FB_MFRC_2, 1, "fb_mfrc_2") \
GEN("ultra_loading_flag", ULTRA_LOADING_FLAG, 1, "ultra_loading_flag")

// generate sysram index list according to FDVFS_V2_COUNTER
#define GEN(name, index, count, var) index,
enum gpu_fdvfs_v2_log_dump {
    GPU_FDVFS_V2_RB_LOG_LIST
    GPU_FDVFS_V2_RB_LOG_MAX,
};
enum gpu_fdvfs_v2_counter {
    GPU_FDVFS_V2_COUNTER_LIST
    GPU_FDVFS_V2_COUNTER_MAX,
};
#undef GEN

// generate sysram address of SYSRAM_GPU_EB_LOG_DUMP_XXXXXX
#define GEN(name, index, count, var) \
	SYSRAM_##index = (AP_FDVFS_DATA_START_OFFSET + index * RB_LOG_COUNT),
enum gpu_sysram_rb_index {
	GPU_FDVFS_V2_RB_LOG_LIST
	SYSRAM_GPU_EB_LOG_DUMP_MAX, // 1KB
};
#undef GEN

// generate sysram address of SYSRAM_GPU_XXXXXX
#define GEN(name, index, count, var) \
	SYSRAM_##index = (FDVFS_NORMAL_VIRTUAL_DATA_START + index),
enum gpu_sysram_index {
	GPU_FDVFS_V2_COUNTER_LIST
	SYSRAM_GPU_EB_COUNTER_MAX,
};
#undef GEN

struct gpu_fdvfs_v2_rb_table_t {
	const char *tag_name;  // only for debug: tag name
	enum gpu_fdvfs_v2_log_dump index;
	unsigned int data_count;
	unsigned int addr;
	const char *var_name; // only for debug: variables included
};

struct gpu_fdvfs_v2_table_t {
	const char *tag_name;  // only for debug: tag name
	enum gpu_fdvfs_v2_counter index;
	unsigned int data_count;
	unsigned int addr;
	const char *var_name; // only for debug: variables included
};

// generate fdvfs_v2_rb_table, index of item in table is the same as enum value
// ex: GPU_EB_LOG_DUMP_COMMIT_REASON1 is store at fdvfs_v2_rb_table[GPU_EB_LOG_DUMP_COMMIT_REASON1]
#define GEN(name, index, count, var_name) {name, index, count, SYSRAM_##index, var_name},
static struct gpu_fdvfs_v2_rb_table_t fdvfs_v2_rb_table[] = {
    GPU_FDVFS_V2_RB_LOG_LIST
};
static struct gpu_fdvfs_v2_table_t fdvfs_v2_table[] = {
    GPU_FDVFS_V2_COUNTER_LIST
};
#undef GEN



/**************************************************
 * SYSRAM For timestamp ringbuffer
 **************************************************/
enum gpu_fdvfs_ts_ringbuffer {
	FASTDVFS_SRAM_TS_0,
	FASTDVFS_SRAM_TS_1,
	FASTDVFS_SRAM_TS_2,
	FASTDVFS_SRAM_TS_3,
	FASTDVFS_SRAM_TS_4,
	FASTDVFS_SRAM_TS_5,
	FASTDVFS_SRAM_TS_6,
	FASTDVFS_SRAM_TS_7,
	FASTDVFS_SRAM_TS_8,
	FASTDVFS_SRAM_TS_9,
	FASTDVFS_SRAM_TS_10,
	FASTDVFS_SRAM_TS_11,
	FASTDVFS_SRAM_TS_12,
	FASTDVFS_SRAM_TS_13,
	FASTDVFS_SRAM_TS_14,
	FASTDVFS_SRAM_TS_15,
	FASTDVFS_SRAM_TS_16,
	FASTDVFS_SRAM_TS_17,
	FASTDVFS_SRAM_TS_18,
	FASTDVFS_SRAM_TS_19,
	FASTDVFS_SRAM_TS_20,
	FASTDVFS_SRAM_TS_21,
	FASTDVFS_SRAM_TS_22,
	FASTDVFS_SRAM_TS_23,
	FASTDVFS_SRAM_TS_24,
	FASTDVFS_SRAM_TS_25,
	FASTDVFS_SRAM_TS_26,
	FASTDVFS_SRAM_TS_27,
	FASTDVFS_SRAM_TS_28,
	FASTDVFS_SRAM_TS_29,
	FASTDVFS_SRAM_TS_30,
	FASTDVFS_SRAM_TS_31,
	FASTDVFS_SRAM_TS_32,
	FASTDVFS_SRAM_TS_33,
	FASTDVFS_SRAM_TS_34,
	FASTDVFS_SRAM_TS_35,
	FASTDVFS_SRAM_TS_36,
	FASTDVFS_SRAM_TS_37,
	FASTDVFS_SRAM_TS_38,
	FASTDVFS_SRAM_TS_39,
	FASTDVFS_SRAM_TS_40,
	FASTDVFS_SRAM_TS_41,
	FASTDVFS_SRAM_TS_42,
	FASTDVFS_SRAM_TS_43,
	FASTDVFS_SRAM_TS_44,
	FASTDVFS_SRAM_TS_45,
	FASTDVFS_SRAM_TS_46,
	FASTDVFS_SRAM_TS_47,
	FASTDVFS_SRAM_TS_48,
	FASTDVFS_SRAM_TS_49,
	FASTDVFS_SRAM_TS_50,
	FASTDVFS_SRAM_TS_51,
	FASTDVFS_SRAM_TS_52,
	FASTDVFS_SRAM_TS_53,
	FASTDVFS_SRAM_TS_54,
	FASTDVFS_SRAM_TS_55,
	FASTDVFS_SRAM_TS_56,
	FASTDVFS_SRAM_TS_57,
	FASTDVFS_SRAM_TS_58,
	FASTDVFS_SRAM_TS_59,
	FASTDVFS_SRAM_TS_60,
	FASTDVFS_SRAM_TS_61,
	FASTDVFS_SRAM_TS_62,
	FASTDVFS_SRAM_TS_63,
	FASTDVFS_SRAM_TS_64,
	FASTDVFS_SRAM_TS_65,
	FASTDVFS_SRAM_TS_66,
	FASTDVFS_SRAM_TS_67,
	FASTDVFS_SRAM_TS_68,
	FASTDVFS_SRAM_TS_69,
	FASTDVFS_SRAM_TS_70,
	FASTDVFS_SRAM_TS_71,
	FASTDVFS_SRAM_TS_72,
	FASTDVFS_SRAM_TS_73,
	FASTDVFS_SRAM_TS_74,
	FASTDVFS_SRAM_TS_75,
	FASTDVFS_SRAM_TS_76,
	FASTDVFS_SRAM_TS_77,
	FASTDVFS_SRAM_TS_78,
	FASTDVFS_SRAM_TS_79,
	FASTDVFS_SRAM_TS_80,
	FASTDVFS_SRAM_TS_81,
	FASTDVFS_SRAM_TS_82,
	FASTDVFS_SRAM_TS_83,
	FASTDVFS_SRAM_TS_84,
	FASTDVFS_SRAM_TS_85,
	FASTDVFS_SRAM_TS_86,
	FASTDVFS_SRAM_TS_87,
	FASTDVFS_SRAM_TS_88,
	FASTDVFS_SRAM_TS_89,
	FASTDVFS_SRAM_TS_90,
	FASTDVFS_SRAM_TS_91,
	FASTDVFS_SRAM_TS_92,
	FASTDVFS_SRAM_TS_93,
	FASTDVFS_SRAM_TS_94,
	FASTDVFS_SRAM_TS_95,
	FASTDVFS_SRAM_TS_96,
	FASTDVFS_SRAM_TS_97,
	FASTDVFS_SRAM_TS_98,
	FASTDVFS_SRAM_TS_99,
	FASTDVFS_SRAM_TS_100,
	FASTDVFS_SRAM_TS_101,
	FASTDVFS_SRAM_TS_102,
	FASTDVFS_SRAM_TS_103,
	FASTDVFS_SRAM_TS_104,
	FASTDVFS_SRAM_TS_105,
	FASTDVFS_SRAM_TS_106,
	FASTDVFS_SRAM_TS_107,
	FASTDVFS_SRAM_TS_108,
	FASTDVFS_SRAM_TS_109,
	FASTDVFS_SRAM_TS_110,
	FASTDVFS_SRAM_TS_111,
	FASTDVFS_SRAM_TS_112,
	FASTDVFS_SRAM_TS_113,
	FASTDVFS_SRAM_TS_114,
	FASTDVFS_SRAM_TS_115,
	FASTDVFS_SRAM_TS_116,
	FASTDVFS_SRAM_TS_117,
	FASTDVFS_SRAM_TS_118,
	FASTDVFS_SRAM_TS_119,
	FASTDVFS_SRAM_TS_120,
	FASTDVFS_SRAM_TS_121,
	FASTDVFS_SRAM_TS_122,
	FASTDVFS_SRAM_TS_123,
	FASTDVFS_SRAM_TS_124,
	FASTDVFS_SRAM_TS_125,
	FASTDVFS_SRAM_TS_126,
	FASTDVFS_SRAM_TS_127
};

#define SYSRAM_GPU_TS_RB_LIST \
	GPU_SRAM_ITEM_TS(0) \
	GPU_SRAM_ITEM_TS(1) \
	GPU_SRAM_ITEM_TS(2) \
	GPU_SRAM_ITEM_TS(3) \
	GPU_SRAM_ITEM_TS(4) \
	GPU_SRAM_ITEM_TS(5) \
	GPU_SRAM_ITEM_TS(6) \
	GPU_SRAM_ITEM_TS(7) \
	GPU_SRAM_ITEM_TS(8) \
	GPU_SRAM_ITEM_TS(9) \
	GPU_SRAM_ITEM_TS(10) \
	GPU_SRAM_ITEM_TS(11) \
	GPU_SRAM_ITEM_TS(12) \
	GPU_SRAM_ITEM_TS(13) \
	GPU_SRAM_ITEM_TS(14) \
	GPU_SRAM_ITEM_TS(15) \
	GPU_SRAM_ITEM_TS(16) \
	GPU_SRAM_ITEM_TS(17) \
	GPU_SRAM_ITEM_TS(18) \
	GPU_SRAM_ITEM_TS(19) \
	GPU_SRAM_ITEM_TS(20) \
	GPU_SRAM_ITEM_TS(21) \
	GPU_SRAM_ITEM_TS(22) \
	GPU_SRAM_ITEM_TS(23) \
	GPU_SRAM_ITEM_TS(24) \
	GPU_SRAM_ITEM_TS(25) \
	GPU_SRAM_ITEM_TS(26) \
	GPU_SRAM_ITEM_TS(27) \
	GPU_SRAM_ITEM_TS(28) \
	GPU_SRAM_ITEM_TS(29) \
	GPU_SRAM_ITEM_TS(30) \
	GPU_SRAM_ITEM_TS(31) \
	GPU_SRAM_ITEM_TS(32) \
	GPU_SRAM_ITEM_TS(33) \
	GPU_SRAM_ITEM_TS(34) \
	GPU_SRAM_ITEM_TS(35) \
	GPU_SRAM_ITEM_TS(36) \
	GPU_SRAM_ITEM_TS(37) \
	GPU_SRAM_ITEM_TS(38) \
	GPU_SRAM_ITEM_TS(39) \
	GPU_SRAM_ITEM_TS(40) \
	GPU_SRAM_ITEM_TS(41) \
	GPU_SRAM_ITEM_TS(42) \
	GPU_SRAM_ITEM_TS(43) \
	GPU_SRAM_ITEM_TS(44) \
	GPU_SRAM_ITEM_TS(45) \
	GPU_SRAM_ITEM_TS(46) \
	GPU_SRAM_ITEM_TS(47) \
	GPU_SRAM_ITEM_TS(48) \
	GPU_SRAM_ITEM_TS(49) \
	GPU_SRAM_ITEM_TS(50) \
	GPU_SRAM_ITEM_TS(51) \
	GPU_SRAM_ITEM_TS(52) \
	GPU_SRAM_ITEM_TS(53) \
	GPU_SRAM_ITEM_TS(54) \
	GPU_SRAM_ITEM_TS(55) \
	GPU_SRAM_ITEM_TS(56) \
	GPU_SRAM_ITEM_TS(57) \
	GPU_SRAM_ITEM_TS(58) \
	GPU_SRAM_ITEM_TS(59) \
	GPU_SRAM_ITEM_TS(60) \
	GPU_SRAM_ITEM_TS(61) \
	GPU_SRAM_ITEM_TS(62) \
	GPU_SRAM_ITEM_TS(63) \
	GPU_SRAM_ITEM_TS(64) \
	GPU_SRAM_ITEM_TS(65) \
	GPU_SRAM_ITEM_TS(66) \
	GPU_SRAM_ITEM_TS(67) \
	GPU_SRAM_ITEM_TS(68) \
	GPU_SRAM_ITEM_TS(69) \
	GPU_SRAM_ITEM_TS(70) \
	GPU_SRAM_ITEM_TS(71) \
	GPU_SRAM_ITEM_TS(72) \
	GPU_SRAM_ITEM_TS(73) \
	GPU_SRAM_ITEM_TS(74) \
	GPU_SRAM_ITEM_TS(75) \
	GPU_SRAM_ITEM_TS(76) \
	GPU_SRAM_ITEM_TS(77) \
	GPU_SRAM_ITEM_TS(78) \
	GPU_SRAM_ITEM_TS(79) \
	GPU_SRAM_ITEM_TS(80) \
	GPU_SRAM_ITEM_TS(81) \
	GPU_SRAM_ITEM_TS(82) \
	GPU_SRAM_ITEM_TS(83) \
	GPU_SRAM_ITEM_TS(84) \
	GPU_SRAM_ITEM_TS(85) \
	GPU_SRAM_ITEM_TS(86) \
	GPU_SRAM_ITEM_TS(87) \
	GPU_SRAM_ITEM_TS(88) \
	GPU_SRAM_ITEM_TS(89) \
	GPU_SRAM_ITEM_TS(90) \
	GPU_SRAM_ITEM_TS(91) \
	GPU_SRAM_ITEM_TS(92) \
	GPU_SRAM_ITEM_TS(93) \
	GPU_SRAM_ITEM_TS(94) \
	GPU_SRAM_ITEM_TS(95) \
	GPU_SRAM_ITEM_TS(96) \
	GPU_SRAM_ITEM_TS(97) \
	GPU_SRAM_ITEM_TS(98) \
	GPU_SRAM_ITEM_TS(99) \
	GPU_SRAM_ITEM_TS(100) \
	GPU_SRAM_ITEM_TS(101) \
	GPU_SRAM_ITEM_TS(102) \
	GPU_SRAM_ITEM_TS(103) \
	GPU_SRAM_ITEM_TS(104) \
	GPU_SRAM_ITEM_TS(105) \
	GPU_SRAM_ITEM_TS(106) \
	GPU_SRAM_ITEM_TS(107) \
	GPU_SRAM_ITEM_TS(108) \
	GPU_SRAM_ITEM_TS(109) \
	GPU_SRAM_ITEM_TS(110) \
	GPU_SRAM_ITEM_TS(111) \
	GPU_SRAM_ITEM_TS(112) \
	GPU_SRAM_ITEM_TS(113) \
	GPU_SRAM_ITEM_TS(114) \
	GPU_SRAM_ITEM_TS(115) \
	GPU_SRAM_ITEM_TS(116) \
	GPU_SRAM_ITEM_TS(117) \
	GPU_SRAM_ITEM_TS(118) \
	GPU_SRAM_ITEM_TS(119) \
	GPU_SRAM_ITEM_TS(120) \
	GPU_SRAM_ITEM_TS(121) \
	GPU_SRAM_ITEM_TS(122) \
	GPU_SRAM_ITEM_TS(123) \
	GPU_SRAM_ITEM_TS(124) \
	GPU_SRAM_ITEM_TS(125) \
	GPU_SRAM_ITEM_TS(126) \
	GPU_SRAM_ITEM_TS(127)

#define GPU_SRAM_ITEM_TS(type) \
	SYSRAM_GPU_TS_RB_##type = (FDVFS_TS_VIRTUAL_DATA_START + FASTDVFS_SRAM_TS_##type * RB_TS_COUNT),

enum FDVFS_SRAM_AP_TS_ADR_LIST {
    SYSRAM_GPU_TS_RB_LIST
   	SYSRAM_GPU_TS_RB_END
};

// For R/W multi variable to single sysram address
union combineData {
	struct {
        unsigned int var1 : 32;
    } oneVar;
	struct {
        unsigned int var1 : 16;
        unsigned int var2 : 16;
    } twoVar;
    struct {
        unsigned int var1 : 8;
        unsigned int var2 : 8;
        unsigned int var3 : 16;
    } thrVar;
    struct {
        unsigned int var1 : 8;
        unsigned int var2 : 8;
        unsigned int var3 : 8;
        unsigned int var4 : 8;
    } fourVar;
    unsigned int value;
};
/******************************************************************
 * API for EB_DVFS_V2
 *****************************************************************/
extern int mtk_gpueb_sysram_rb_write(int rb_num, GPU_TS_INFO ts_in);
extern union combineData mtk_gpueb_sysram_multi_read(int offset);
extern struct GED_DVFS_OPP_STAT mtk_gpueb_mbrain_read(int opp);

/**************************************************
 * Platform Implementation
 **************************************************/
struct ged_platform_fp {
	/* Common */
	unsigned int (*get_sysram)(int type);
	unsigned int (*get_ts_rb_num)(void);
	unsigned int (*get_mbrain_max_num)(void);
};

void ged_register_platform_fp(struct ged_platform_fp *platform_fp);
void ged_do_platform_related_init(void);
unsigned int ged_get_ts_rb_num(void);
unsigned int ged_get_mbrain_max_num(void);

#endif // __GED_EB_H__
