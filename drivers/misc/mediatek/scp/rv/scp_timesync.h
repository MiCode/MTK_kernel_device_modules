/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SCP_TIMESYNC_H_
#define _SCP_TIMESYNC_H_

#include "scp_helper.h"

#define SCP_TS_MBOX_OFFSET_BASE     (scpreg.timesync_mbox)

/*
 * Shared MBOX: AP write, SCP read
 * Unit for each offset: 4 bytes
 */

#define SCP_TS_MBOX_TICK_H          (SCP_TS_MBOX_OFFSET_BASE + 0)
#define SCP_TS_MBOX_TICK_L          (SCP_TS_MBOX_OFFSET_BASE + 4)
#define SCP_TS_MBOX_TS_H            (SCP_TS_MBOX_OFFSET_BASE + 8)
#define SCP_TS_MBOX_TS_L            (SCP_TS_MBOX_OFFSET_BASE + 12)
#define SCP_TS_MBOX_DEBUG_TS_H      (SCP_TS_MBOX_OFFSET_BASE + 14)
#define SCP_TS_MBOX_DEBUG_TS_L      (SCP_TS_MBOX_OFFSET_BASE + 16)

void scp_ready_timesync(void);
void scp_timesync_suspend(void);
void scp_timesync_resume(void);
unsigned int scp_timesync_init(void);
extern bool scp_timesync_flag;

#endif // _SCP_TIMESYNC_H_
