/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_DEC_PM_PLAT_H_
#define _MTK_VCODEC_DEC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

#define DEC_DVFS	1
#define DEC_EMI_BW	1
#define MTK_VDEC_CHECK_ACTIVE_INTERVAL 2000 // ms

#ifdef DEC_DVFS
#define VDEC_CHECK_ALIVE 1 /* vdec check alive have to enable DEC_DVFS first */
#endif

/*
 * enum vdec_dvfs_checklist_item - The checkitem for vdec dvfs preparing
 * @VDEC_DVFS_CHECKLIST_DVFS_QOS_VERSION:    dvfs-qos-ver
 * @VDEC_DVFS_CHECKLIST_MMDVFS_IN_VCP:       vdec-mmdvfs-in-vcp
 * @VDEC_DVFS_CHECKLIST_MMDVFS_IN_ADAPTIVE:  vdec-mmdvfs-in-adaptive
 * @VDEC_DVFS_CHECKLIST_SET_BW_IN_MIN_FREQ:  vdec-set-bw-in-min-freq
 * @VDEC_DVFS_CHECKLIST_CPU_HINT_MODE:       vdec-cpu-hint-mode
 * @VDEC_DVFS_CHECKLIST_THERMAL_HINT_MODE:   vdec-thermal-hint-mode
 * @VDEC_DVFS_CHECKLIST_REGULATOR_MODE:      regulator mode  (0 for failure, 1 for success)
 * @VDEC_DVFS_CHECKLIST_MMDVFS_CLK_MODE:     mmdvfs_clk mode (0 for failure, 1 for success)
 * @VDEC_DVFS_CHECKLIST_THROUGHPUT_OP_RATE_THRESH: throughput-op-rate-thresh (0 for not set, 1 for dts value)
 * @VDEC_DVFS_CHECKLIST_THROUGHPUT_MIN:      throughput-min (0 for not set, 1 for dts value)
 * @VDEC_DVFS_CHECKLIST_THROUGHPUT_NORMAL_MAX:   throughput-normal-max (0 for not set, 1 for dts value)
 * @VDEC_DVFS_CHECKLIST_MAX_OP_RATE_ITEM_NUM:    max-op-rate-item-num (0 for not set, 1 for dts value)
 * @VDEC_DVFS_CHECKLIST_OS_ALLOW_BW:             os-allow-bw (0 for not set, 1 for dts value)
 */
enum vdec_dvfs_checklist_item {
	VDEC_DVFS_CHECKLIST_DVFS_QOS_VERSION = 0,
	VDEC_DVFS_CHECKLIST_MMDVFS_IN_VCP = 1,
	VDEC_DVFS_CHECKLIST_MMDVFS_IN_ADAPTIVE = 2,
	VDEC_DVFS_CHECKLIST_SET_BW_IN_MIN_FREQ = 3,
	VDEC_DVFS_CHECKLIST_CPU_HINT_MODE = 4,
	VDEC_DVFS_CHECKLIST_THERMAL_HINT_MODE = 5,
	VDEC_DVFS_CHECKLIST_REGULATOR_MODE = 6,
	VDEC_DVFS_CHECKLIST_MMDVFS_CLK_MODE = 7,
	VDEC_DVFS_CHECKLIST_THROUGHPUT_OP_RATE_THRESH = 8,
	VDEC_DVFS_CHECKLIST_THROUGHPUT_MIN = 9,
	VDEC_DVFS_CHECKLIST_THROUGHPUT_NORMAL_MAX = 10,
	VDEC_DVFS_CHECKLIST_MAX_OP_RATE_ITEM_NUM = 11,
	VDEC_DVFS_CHECKLIST_OS_ALLOW_BW = 12,
	VDEC_DVFS_CHECKLIST_NUM = 13,
};

void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_vdec_force_update_freq(struct mtk_vcodec_dev *dev);

void mtk_vdec_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_dvfs_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id);
void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in);
void mtk_vdec_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in);
void mtk_vdec_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_sync_boost_data(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_dvfs_update_dvfs_params(struct mtk_vcodec_ctx *ctx);
bool mtk_vdec_dvfs_monitor_op_rate(struct mtk_vcodec_ctx *ctx, int buf_type);
#endif /* _MTK_VCODEC_DEC_PM_PLAT_H_ */
