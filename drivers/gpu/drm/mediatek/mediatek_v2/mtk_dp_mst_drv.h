/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DP_MST_DRV_H__
#define __MTK_DP_MST_DRV_H__

#define DPTX_CTP			0
#define DPTX_LINUX			1
#define DPTX_OS				DPTX_LINUX

#include "mtk_dp_common.h"

#define ENABLE_DPTX_MST_DEBUG 0

#define DPTX_EDID_SIZE		0x200
#define I2C_M_STOP		0x8000	/* use only if I2C_FUNC_PROTOCOL_MANGLING */

#define DPTX_PHY_HPD_INT_EVNET		HPD_INT_EVNET
#define DPTX_PHY_HPD_CONNECT		HPD_CONNECT
#define DPTX_PHY_HPD_DISCONNECT		HPD_DISCONNECT
#define DPTX_PHY_HPD_INITIAL_STATE	HPD_INITIAL_STATE

#define DPTX_SUPPORT_MAX_LINKRATE	DP_LINKRATE_HBR2
#define DPTX_SUPPORT_MAX_LANECOUNT	DP_LANECOUNT_4

#define DPTX_MST_I2C_READ_ENABLE 1
#define DPTX_MST_FEC_ENABLE 1
#define DPTX_MST_POWER_UP_PHY_ENABLE 1
#define DPTX_MST_HDCP_ENABLE 0
#define DPTX_MST_DEBUG 1
#define DP_PAYLOAD_START_SLOT 1
#define DP_PAYLOAD_START_SLOT_OFFSET 0

#define DPTX_PAYLOAD_MAX 8
#define DPTX_LCT_MAX (DPTX_STREAM_MAX * 2)
#define DPTX_PORT_NUM_MAX (DP_MST_LOGICAL_PORT_0 + DPTX_STREAM_MAX)

/* audio m divider will change by project */
enum DPTX_AUDIO_M_DIV {
	DPTX_AUDIO_M_DIV_M1	= 0x0,
	DPTX_AUDIO_M_DIV_M2	= 0x1,
	DPTX_AUDIO_M_DIV_M4	= 0x2,
	DPTX_AUDIO_M_DIV_M8	= 0x3,
	DPTX_AUDIO_M_DIV_D2	= 0x4,
	DPTX_AUDIO_M_DIV_D4	= 0x5,
	DPTX_AUDIO_M_DIV_NA	= 0x6,
	DPTX_AUDIO_M_DIV_D8	= 0x7,
};

#if (DPTX_OS == DPTX_LINUX)
struct mtk_dp_mst_connector {
	struct mtk_dp *mtk_dp;
	struct drm_dp_mst_port *port;
	struct drm_connector connector;

	struct drm_display_mode *native;
	struct edid *edid;
};
#endif

#if (DPTX_OS == DPTX_CTP)
BYTE *mtk_dptx_mst_drv_send_remote_i2c_read (struct drm_dp_mst_branch *mstb, struct drm_dp_mst_port *port);
void mtk_dptx_drv_video_clock(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id);
#endif

void mtk_dptx_drv_set_pg_mode(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable, u8 pattern_id);
void mtk_dptx_drv_set_pg_timing_info(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 pattern_id);
void mtk_dptx_drv_video_config(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id);
void mtk_dptx_drv_i2s_audio_set_mdiv(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 divider);
void mtk_dptx_drv_i2s_audio_sdp_setting(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					u8 ch,
					u8 fs,
					u8 len);
void mtk_dptx_drv_i2s_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				u8 ch, u8 fs, u8 len);
void mtk_dptx_drv_dsc_pps_send(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 *pps_128);



void mtk_dptx_mst_drv_reset(struct mtk_dp *mtk_dp, u8 is_plug);
u8 mtk_dptx_mst_drv_handler(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_drv_sideband_msg_irq_clear(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_drv_sideband_msg_rdy_clear(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_drv_stop(struct mtk_dp *mtk_dp);
#endif
