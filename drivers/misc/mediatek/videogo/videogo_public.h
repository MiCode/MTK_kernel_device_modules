/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef VIDEOGO_PUBLIC_H
#define VIDEOGO_PUBLIC_H

enum videogo_data_type {
	VGO_SEND_UPDATE_FN,
	VGO_SEND_OPRATE,
	VGO_RECV_INSTANCE_INC,
	VGO_RECV_INSTANCE_DEC,
	VGO_RECV_RUNNING_UPDATE,
	VGO_RECV_STATE_OPEN
};

// VGO_RECV_INSTANCE_INC
// VGO_RECV_INSTANCE_DEC
struct inst_init_data {
	int inst_type;      /* VDEC/VENC */
	int ctx_id;
	int caller_pid;
	unsigned int fourcc;
	int oprate;         /* Set from Framework */
	int width;
	int height;
};

// VGO_RECV_RUNNING_UPDATE
struct inst_data {
	int inst_type;      /* VDEC/VENC */
	int ctx_id;
	int oprate;         /* Set from AVDVFS */
	int hw_proc_time[3];
};

// VGO_SEND_OPRATE
// VGO_RECV_STATE_OPEN
struct oprate_data {
	int inst_type;      /* VDEC/VENC */
	int ctx_id;
	int oprate;         /* Set from VideoGo */
};

struct vgo_data {
	int count;          /* Number of oprate_data instances */
	struct oprate_data *data;
};

#endif // VIDEOGO_PUBLIC_H
