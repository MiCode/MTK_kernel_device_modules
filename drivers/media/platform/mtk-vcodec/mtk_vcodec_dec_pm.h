/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_DEC_PM_H_
#define _MTK_VCODEC_DEC_PM_H_

#include "mtk_vcodec_drv.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
void vdec_dump_mem_buf(unsigned long h_vdec);
#endif

void mtk_dec_init_ctx_pm(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_init_dec_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_release_dec_pm(struct mtk_vcodec_dev *dev);

void mtk_vcodec_dec_pw_on(struct mtk_vcodec_pm *pm);
void mtk_vcodec_dec_pw_off(struct mtk_vcodec_pm *pm);
void mtk_vcodec_dec_clock_on(struct mtk_vcodec_pm *pm, unsigned int hw_id);
void mtk_vcodec_dec_clock_off(struct mtk_vcodec_pm *pm, unsigned int hw_id);
void mtk_vdec_uP_TF_dump_handler(struct work_struct *ws);
void mtk_vdec_translation_fault_callback_setting(struct mtk_vcodec_dev *dev);
int mtk_vdec_m4u_port_name_to_index(const char *name);

extern void mtk_vdec_do_gettimeofday(struct timespec64 *tv);

#endif /* _MTK_VCODEC_DEC_PM_H_ */
