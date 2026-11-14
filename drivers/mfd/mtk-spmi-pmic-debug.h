/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_SPMI_PMIC_DEBUG_H__
#define __MTK_SPMI_PMIC_DEBUG_H__

#define PMIC_HWCID_E2                   (0x20)
#define PMIC_MBRAIN_VER_INFO_IDX        0
#define spmi_glitch_idx_cnt             65
#define spmi_parity_err_idx_cnt         33
#define spmi_crc_err_idx_cnt            33
#define PMIC_PRE_OT_CNT_NUM             1
#define PMIC_PRE_LVSYS_CNT_NUM          1
#define PMIC_CURR_CLAMPING_CNT_NUM      6
#define PMIC_PRE_OT_BUF_SIZE            ((PMIC_PRE_OT_CNT_NUM * SPMI_MAX_SLAVE_ID) + 1)
#define PMIC_PRE_LVSYS_BUF_SIZE         ((PMIC_PRE_LVSYS_CNT_NUM * SPMI_MAX_SLAVE_ID) + 1)
#define PMIC_CURR_CLAMPING_BUF_SIZE     ((PMIC_CURR_CLAMPING_CNT_NUM * SPMI_MAX_SLAVE_ID) + 1)
#define SPMI_MIN_SLAVE_ID               2

#define SPMI_PMIC_DEBUG_RG_NUM          9
#define SPMI_PMIC_DEBUG_RG_BUF_SIZE     ((SPMI_PMIC_DEBUG_RG_NUM * SPMI_MAX_SLAVE_ID) + 1)
#define UCHAR_MAX                       255U

extern void mtk_spmi_pmic_get_glitch_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_parity_err_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_pre_ot_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_pre_lvsys_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_current_clamping_cnt(u16 *buf);
extern void mtk_spmi_pmic_get_debug_rg_info(u32 *buf);
extern void mtk_spmi_pmic_get_crc_err_cnt(u16 *buf);

/* spmi pmic dump data */
enum dump_rg {
	RGS_NPKT_CCLP_ERR,
	MAX_DUMP_RG_NUM,
};
extern int mtk_spmi_pmic_dump_rg_data(u8 slvid, u32 *rdata, enum dump_rg rg_name);

MODULE_AUTHOR("HS Chien <HS.Chien@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SPMI PMIC");
MODULE_LICENSE("GPL");

#endif /*__MTK_SPMI_PMIC_DEBUG_H__*/
