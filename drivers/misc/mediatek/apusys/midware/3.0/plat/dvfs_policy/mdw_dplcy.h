/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_DVFS_POLICY_H__
#define __MTK_APU_MDW_DVFS_POLICY_H__

#define POLICY_OPP_FINE_TUNE (0)

int mdw_dplcy_session_create(struct mdw_fpriv *mpriv);
void mdw_dplcy_session_delete(struct mdw_fpriv *mpriv);
int mdw_dplcy_init(struct mdw_device *mdev);
void mdw_dplcy_deinit(struct mdw_device *mdev);

#endif