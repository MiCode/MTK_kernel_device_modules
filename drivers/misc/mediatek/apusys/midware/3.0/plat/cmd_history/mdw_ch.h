/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_CMD_HISTORY_H__
#define __MTK_APU_MDW_CMD_HISTORY_H__

int mdw_ch_pollcmd_timeout(uint32_t *flag, uint32_t poll_interval_us, uint32_t poll_timeout_us,
	struct mdw_cmd *c);
int mdw_ch_cmd_create_tbl(struct mdw_cmd *c);
bool mdw_ch_cmd_exec_update(struct mdw_cmd *c);
int mdw_ch_session_create(struct mdw_fpriv *mpriv);
void mdw_ch_session_delete(struct mdw_fpriv *mpriv);
int mdw_ch_init(struct mdw_device *mdev);
void mdw_ch_deinit(struct mdw_device *mdev);

#endif
