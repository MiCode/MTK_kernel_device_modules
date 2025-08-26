/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_ENC_PM_PLAT_H_
#define _MTK_VCODEC_ENC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

#define ENC_DVFS	1
#define ENC_EMI_BW	1

/*
 * enum venc_dvfs_checklist_item - The checkitem for venc dvfs preparing
 * @VENC_DVFS_CHECKLIST_DVFS_QOS_VERSION:    dvfs-qos-ver
 * @VENC_DVFS_CHECKLIST_MMDVFS_IN_VCP:       venc-mmdvfs-in-vcp
 * @VENC_DVFS_CHECKLIST_MMDVFS_IN_ADAPTIVE:  venc-mmdvfs-in-adaptive
 * @VENC_DVFS_CHECKLIST_CPU_HINT_MODE:       venc-cpu-hint-mode
 * @VENC_DVFS_CHECKLIST_THERMAL_HINT_MODE:   venc-thermal-hint-mode
 * @VENC_DVFS_CHECKLIST_REGULATOR_MODE:      regulator mode  (0 for failure, 1 for success)
 * @VENC_DVFS_CHECKLIST_MMDVFS_CLK_MODE:     mmdvfs_clk mode (0 for failure, 1 for success)
 * @VENC_DVFS_CHECKLIST_THROUGHPUT_OP_RATE_THRESH: throughput-op-rate-thresh (0 for not set, 1 for dts value)
 * @VENC_DVFS_CHECKLIST_THROUGHPUT_MIN:      throughput-min (0 for not set, 1 for dts value)
 * @VENC_DVFS_CHECKLIST_THROUGHPUT_NORMAL_MAX:    throughput-normal-max (0 for not set, 1 for dts value)
 * @VENC_DVFS_CHECKLIST_THROUGHPUT_CONFIG_OFFSET: throughput-config-offset (0 for not set, 1 for dts value)
 */
enum venc_dvfs_checklist_item {
	VENC_DVFS_CHECKLIST_DVFS_QOS_VERSION = 0,
	VENC_DVFS_CHECKLIST_MMDVFS_IN_VCP = 1,
	VENC_DVFS_CHECKLIST_MMDVFS_IN_ADAPTIVE = 2,
	VENC_DVFS_CHECKLIST_CPU_HINT_MODE = 3,
	VENC_DVFS_CHECKLIST_THERMAL_HINT_MODE = 4,
	VENC_DVFS_CHECKLIST_REGULATOR_MODE = 5,
	VENC_DVFS_CHECKLIST_MMDVFS_CLK_MODE = 6,
	VENC_DVFS_CHECKLIST_THROUGHPUT_OP_RATE_THRESH = 7,
	VENC_DVFS_CHECKLIST_THROUGHPUT_MIN = 8,
	VENC_DVFS_CHECKLIST_THROUGHPUT_NORMAL_MAX = 9,
	VENC_DVFS_CHECKLIST_THROUGHPUT_CONFIG_OFFSET = 10,
	VENC_DVFS_CHECKLIST_NUM = 11,
};

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_venc_dvfs_reset_vsi_data(struct mtk_vcodec_dev *dev);
void mtk_venc_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx);
void mtk_venc_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param);
void mtk_venc_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param);
void mtk_venc_pmqos_lock_unlock(struct mtk_vcodec_dev *dev, bool is_lock);

void mtk_venc_pmqos_monitor(struct mtk_vcodec_dev *dev, u32 state);
void mtk_venc_pmqos_monitor_init(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_monitor_deinit(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_monitor_reset(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_frame_req(struct mtk_vcodec_ctx *ctx, bool start);
bool mtk_venc_dvfs_monitor_op_rate(struct mtk_vcodec_ctx *ctx, int buf_type);
void mtk_venc_dvfs_check_boost(struct mtk_vcodec_dev *dev);
void mtk_venc_init_boost(struct mtk_vcodec_ctx *ctx);
#endif /* _MTK_VCODEC_ENC_PM_H_ */
