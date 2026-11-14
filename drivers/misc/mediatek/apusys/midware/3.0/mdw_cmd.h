/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APUCMD_H__
#define __MTK_APUCMD_H__

#include "mdw_cmn.h"

/* cmd idr range */
#define MDW_CMD_IDR_MIN (1)
#define MDW_CMD_IDR_MAX (64)

/* stale cmd timeout */
#define MDW_STALE_CMD_TIMEOUT (5*1000) //ms

/* poll cmd */
#define MDW_POLL_TIMEOUT (4*1000) //us
#define MDW_POLLTIME_SLEEP_TH(x) (x*65/100) //us

/* cmd deubg */
#define mdw_cmd_show(c, f) \
	f("cmd(0x%llx/0x%llx/0x%llx/0x%llx/%d/%u)param(%u/%u/%u/%u/"\
	"%u/%u/%u/%u/%u/%llu)subcmds(%u/%pK/%u/%u)pid(%d/%d)(%d)\n", \
	(uint64_t)c->mpriv, c->uid, c->kid, c->inference_id, c->id, kref_read(&c->ref), \
	c->priority, c->hardlimit, c->softlimit, \
	c->power_save, c->power_plcy, c->power_dtime, \
	c->app_type, c->inference_ms, c->tolerance_ms, c->is_dtime_set, \
	c->num_subcmds, c->cmdbufs, c->num_cmdbufs, c->size_cmdbufs, \
	c->pid, c->tgid, task_pid_nr(current))

/* cmd map */
int mdw_cmd_invoke_map(struct mdw_cmd *c, struct mdw_mem_map *map);
void mdw_cmd_unvoke_map(struct mdw_cmd *c);

void mdw_cmd_release_session(struct mdw_fpriv *mpriv);

#endif
