/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_MBRAIN_H__
#define __MTK_MBRAIN_H__

struct mb_work_struct_data {
	void *data;
	struct work_struct task;
};

enum MB_DISP_REQ_TYPE {
	DISP_JUST_NOTIFY_TO_MB = 0,
	DISP_SCRAP_YOCTO_TRACE = 1,
	DISP_SCRAP_ANDROID_TRACE = 2,
	DISP_SCRAP_YOCTO_ANDROID_TRACE = 3,
	DISP_SCRAP_YOCTO_ANDROID_HYP_TRACE = 4,
	MB_DISP_REQ_TYPE_MAX,
};

enum MB_DISP_EVENT {
	DISP_DSI_UNDERRUN = 0,
	DISP_EXDMA_UNDERRUN = 1,
	DISP_DDP_CMDQ_CB_TIMEOUT = 2,
	DISP_MTK_CRTC_GCE_FLUSH_TIMEOUT = 3,
	DISP_MTK_ATOMIC_COMMIT_TIMEOUT = 4,
	DISP_EVENT_MAX,
};

struct mb_disp_data {
	unsigned long disp_clk_normal;
	unsigned int disp_power_normal;
	unsigned long disp_clk_cur;
	unsigned int disp_power_cur;
	unsigned int mtk_crtc_id;
	enum MB_DISP_EVENT event;
	enum MB_DISP_REQ_TYPE req_type;
	char identity_info[128];
};

int disp_mb_register(void (*callback)(enum MB_DISP_EVENT event, void *data));
int disp_mb_unregister(void);
int disp_mb_event_trigger(struct mb_disp_data *mb_data);

#endif	/* __MTK_MBRAIN_H__ */
