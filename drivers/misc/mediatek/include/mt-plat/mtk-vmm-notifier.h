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

enum {
	CVFS_NULL_POINTER = -5,
	CVFS_UPDATE_EXCEPTION = -4,
	CVFS_INVALID_PARAM = -3,
	CVFS_NEGATIVE_OVERFLOW = -2,
	CVFS_POSITIVE_OVERFLOW = -1,
	CVFS_UPDATE_SUCCESS = 0,
	CVFS_ENABLE_TOGGLE = 1,
	CVFS_DISABLE_TOGGLE = 2,
};

enum VMM_CVFS_USR_ID {
	VMM_CVFS_USR_START,
	VMM_CVFS_USR_CAMSYS = VMM_CVFS_USR_START,
	VMM_CVFS_USR_IMGSYS,
	VMM_CVFS_USR_PDA,
	VMM_CVFS_USR_SENINF,
	VMM_CVFS_USR_UISP,
	VMM_CVFS_USR_VDE,
	VMM_CVFS_USR_MAX,
};

int vmm_isp_ctrl_notify(int openIsp);
int vmm_enable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id);
int vmm_disable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id);
int vmm_cvfs_dump(void);
int mtk_vmm_ctrl_dbg_use(bool enable);

#endif
