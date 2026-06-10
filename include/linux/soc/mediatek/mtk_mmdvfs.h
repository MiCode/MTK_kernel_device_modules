/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MTK_MMDVFS_H
#define __SOC_MTK_MMDVFS_H

#include <linux/kernel.h>

typedef void (*record_opp)(const u8 opp);
typedef void (*ap_ccf)(const bool enable);

#if IS_ENABLED(CONFIG_MTK_MMDVFS)

/* For systrace */
bool mmdvfs_systrace_enabled(void);
int mmdvfs_tracing_mark_write(char *fmt, ...);

#define TRACE_MSG_LEN	1024

#define MMDVFS_TRACE_FORCE_BEGIN_TID(tid, fmt, args...) \
	mmdvfs_tracing_mark_write("B|%d|" fmt "\n", tid, ##args)

#define MMDVFS_TRACE_FORCE_BEGIN(fmt, args...) \
	MMDVFS_TRACE_FORCE_BEGIN_TID(current->tgid, fmt, ##args)

#define MMDVFS_TRACE_FORCE_END() \
	mmdvfs_tracing_mark_write("E\n")

#define MMDVFS_SYSTRACE_BEGIN(fmt, args...) do { \
	if (mmdvfs_systrace_enabled()) { \
		MMDVFS_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define MMDVFS_SYSTRACE_END() do { \
	if (mmdvfs_systrace_enabled()) { \
		MMDVFS_TRACE_FORCE_END(); \
	} \
} while (0)

int register_mmdvfs_notifier(struct notifier_block *nb);
int unregister_mmdvfs_notifier(struct notifier_block *nb);
int mmdvfs_set_ccf_enable_mutex(bool lock);
int mmdvfs_set_vcp_cb_ready(bool enable);
int mmdvfs_ap_ccf_enable(bool enable);
int mmdvfs_set_force_step(int force_step);
int mmdvfs_set_vote_step(int vote_step);
void mmdvfs_debug_record_opp_set_fp(record_opp fp);
void mmdvfs_ap_ccf_enable_notifier_set_fp(ap_ccf fp);
void mtk_mmdvfs_aov_enable(bool enable);
#else
static inline int register_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }
static inline int unregister_mmdvfs_notifier(struct notifier_block *nb)
{ return -EINVAL; }
static inline int mmdvfs_set_ccf_enable_mutex(bool lock)
{ return 0; }
static inline int mmdvfs_set_vcp_cb_ready(bool enable)
{ return 0; }
static inline int mmdvfs_ap_ccf_enable(bool enable)
{ return 0; }
static inline int mmdvfs_set_force_step(int new_force_step)
{ return 0; }
static inline int mmdvfs_set_vote_step(int new_force_step)
{ return 0; }
static inline void mmdvfs_debug_record_opp_set_fp(record_opp fp) {}
static inline void mmdvfs_ap_ccf_enable_notifier_set_fp(ap_ccf fp) {}
static inline void mtk_mmdvfs_aov_enable(bool enable) {}
#endif /* CONFIG_MTK_MMDVFS */

#endif
