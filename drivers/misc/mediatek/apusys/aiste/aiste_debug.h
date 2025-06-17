/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __AISTE_DEBUG_H__
#define __AISTE_DEBUG_H__

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#endif

enum AISTE_DEBUG_MASK {
	AISTE_DBG_DRV         = (1U << 0),
	AISTE_DBG_QOS         = (1U << 1),
	AISTE_DBG_ALL         = (1U << 2) - 1,
};

extern u32 g_uclamp_min;
extern u32 g_uclamp_max;
extern u32 g_klog;

#define redirect_output(...) pr_info(__VA_ARGS__)

/* log level : error */
#define aiste_err(...) redirect_output(__VA_ARGS__)

/* log level : debug */
#define aiste_debug(mask, ...)                  \
do {                                            \
	if ((mask & g_klog))    \
		redirect_output(__VA_ARGS__);           \
} while (0)

#define aiste_drv_debug(...) aiste_debug(AISTE_DBG_DRV, __VA_ARGS__)
#define aiste_qos_debug(...) aiste_debug(AISTE_DBG_QOS, __VA_ARGS__)

int aiste_procfs_init(void);
void aiste_procfs_remove(void);

#endif /* __AISTE_DEBUG_H__ */
