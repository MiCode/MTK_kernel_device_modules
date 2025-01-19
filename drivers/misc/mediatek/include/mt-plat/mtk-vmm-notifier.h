/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __MTK_VMM_NOTIFIER__
#define __MTK_VMM_NOTIFIER__

/**
 * @brief Notifiy isp status
 *
 * @param openIsp 1: isp on 0: isp off
 * @return int
 */

enum VMM_CVFS_SEL_ID {
    VMM_CVFS_SEL_ID_START,
    VMM_CVFS_CAM_SEL = VMM_CVFS_SEL_ID_START,
    VMM_CVFS_IMG_SEL,
    VMM_CVFS_IPE_SEL,
    VMM_CVFS_VDE_SEL,
    VMM_CVFS_SEL_ID_END,
};

int vmm_isp_ctrl_notify(int openIsp);
int vmm_enable_cvfs(int user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id);
int vmm_disable_cvfs(int user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id);
int vmm_cvfs_dump(void);

#endif
