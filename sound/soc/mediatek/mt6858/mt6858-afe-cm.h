/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Shawn Sung <shawn.sung@mediatek.com>
 */

#ifndef MTK_AFE_CM_H_
#define MTK_AFE_CM_H_
enum {
	CM0,
	CM1,
	CM_NUM,
};

void mt6858_set_cm_rate(int id, unsigned int rate);
void mt6858_set_cm_mux(int id, unsigned int mux);
int mt6858_get_cm_mux(int id);
int mt6858_set_cm(struct mtk_base_afe *afe, int id, unsigned int update,
				bool swap, unsigned int ch);
int mt6858_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en);
int mt6858_cm_output_mux(struct mtk_base_afe *afe, int id, bool sel);
int mt6858_enable_cm(struct mtk_base_afe *afe, int id, bool en);
int mt6858_is_need_enable_cm(struct mtk_base_afe *afe, int id);

#endif /* MTK_AFE_CM_H_ */

