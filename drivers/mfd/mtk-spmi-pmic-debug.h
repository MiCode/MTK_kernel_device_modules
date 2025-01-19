/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_SPMI_PMIC_DEBUG_H__
#define __MTK_SPMI_PMIC_DEBUG_H__

#define spmi_glitch_idx_cnt 65
#define spmi_parity_err_idx_cnt 33
extern void mtk_spmi_pmic_get_glitch_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_parity_err_cnt(u16 *buf);

MODULE_AUTHOR("HS Chien <HS.Chien@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SPMI PMIC");
MODULE_LICENSE("GPL");

#endif /*__MTK_SPMI_PMIC_DEBUG_H__*/