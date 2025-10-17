/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2024 MediaTek Inc.
 */
#define spmi_glitch_idx_cnt 65
extern void mtk_spmi_pmic_get_glitch_cnt(u16 *buf);

MODULE_AUTHOR("HS Chien <HS.Chien@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SPMI PMIC");
MODULE_LICENSE("GPL");
