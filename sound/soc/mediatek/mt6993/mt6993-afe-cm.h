/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */

#ifndef MTK_AFE_CM_H_
#define MTK_AFE_CM_H_
enum {
	CM0,
	CM1,
	CM2,
	CM_NUM,
};

void mt6993_set_cm_rate(int id, unsigned int rate);
void mt6993_set_cm_mux(int id, unsigned int mux);
int mt6993_get_cm_mux(int id);
int mt6993_set_cm(struct mtk_base_afe *afe, int id, unsigned int update,
				bool swap, unsigned int ch);
int mt6993_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en);
int mt6993_cm_output_mux(struct mtk_base_afe *afe, int id, bool sel);
int mt6993_enable_cm(struct mtk_base_afe *afe, int id, bool en);
int mt6993_is_need_enable_cm(struct mtk_base_afe *afe, int id);

#endif /* MTK_AFE_CM_H_ */

