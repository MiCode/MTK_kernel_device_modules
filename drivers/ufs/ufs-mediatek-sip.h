/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_SIP_H
#define _UFS_MEDIATEK_SIP_H

#include <linux/soc/mediatek/mtk_sip_svc.h>

/*
 * SiP commands
 */
#define MTK_SIP_UFS_CONTROL               MTK_SIP_SMC_CMD(0x276)
#define UFS_MTK_SIP_VA09_PWR_CTRL         BIT(0)
#define UFS_MTK_SIP_DEVICE_RESET          BIT(1)
#define UFS_MTK_SIP_CRYPTO_CTRL           BIT(2)
#define UFS_MTK_SIP_REF_CLK_NOTIFICATION  BIT(3)
#define UFS_MTK_SIP_SRAM_PWR_CTRL         BIT(5)
#define UFS_MTK_SIP_GET_VCC_NUM           BIT(6)
#define UFS_MTK_SIP_DEVICE_PWR_CTRL       BIT(7)
#define UFS_MTK_SIP_MPHY_CTRL             BIT(8)
#define UFS_MTK_SIP_MTCMOS_CTRL           BIT(9)
#define UFS_MTK_SIP_RPMB_KEY              BIT(10)
/*
 * Multi-VCC by Numbering
 */
enum ufs_mtk_vcc_num {
	UFS_VCC_NONE = 0,
	UFS_VCC_1,
	UFS_VCC_2,
	UFS_VCC_MAX
};

enum ufs_mtk_mphy_op {
	UFS_MPHY_BACKUP = 0,
	UFS_MPHY_RESTORE
};

/*
 * SMC call wapper function
 */
struct ufs_mtk_smc_arg {
	unsigned long cmd;
	struct arm_smccc_res *res;
	unsigned long v1;
	unsigned long v2;
	unsigned long v3;
	unsigned long v4;
	unsigned long v5;
	unsigned long v6;
	unsigned long v7;
};
#if IS_ENABLED(CONFIG_UFS_MEDIATEK_MT6771)
enum bc_flags_bits {
	__BC_CRYPT,        /* marks the request needs crypt */
	__BC_IV_PAGE_IDX,  /* use page index as iv. */
	__BC_IV_CTX,       /* use the iv saved in crypt context */
	__BC_AES_128_XTS,  /* crypt algorithms */
	__BC_AES_192_XTS,
	__BC_AES_256_XTS,
	__BC_AES_128_CBC,
	__BC_AES_256_CBC,
	__BC_AES_128_ECB,
	__BC_AES_256_ECB,
};

#define BC_CRYPT	(1UL << __BC_CRYPT)
#define BC_IV_PAGE_IDX  (1UL << __BC_IV_PAGE_IDX)
#define BC_IV_CTX       (1UL << __BC_IV_CTX)
#define BC_AES_128_XTS	(1UL << __BC_AES_128_XTS)
#define BC_AES_192_XTS	(1UL << __BC_AES_192_XTS)
#define BC_AES_256_XTS	(1UL << __BC_AES_256_XTS)
#define BC_AES_128_CBC	(1UL << __BC_AES_128_CBC)
#define BC_AES_256_CBC	(1UL << __BC_AES_256_CBC)
#define BC_AES_128_ECB	(1UL << __BC_AES_128_ECB)
#define BC_AES_256_ECB	(1UL << __BC_AES_256_ECB)

#define UFS_HIE_PARAM_OFS_CFG_ID         (24)
#define UFS_HIE_PARAM_OFS_MODE           (16)
#define UFS_HIE_PARAM_OFS_KEY_TOTAL_BYTE (8)
#define UFS_HIE_PARAM_OFS_KEY_START_BYTE (0)
#define UFS_MTK_SIP_HIE_CFG_REQUEST       MTK_SIP_SMC_CMD(0x274)
/*Need a session id to intercept other false calls --- Ufs Program Key Request*/
#define UFS_CRYPTO_SESSION_ID            (0x55504B52)

static inline void _ufs_mtk_old_smc( struct ufs_mtk_smc_arg s)
{
	arm_smccc_smc(
		s.cmd,
		s.v1, s.v2, s.v3, s.v4, s.v5, s.v6, s.v7,s.res);
}

#define ufs_mtk_old_smc(...) \
	_ufs_mtk_old_smc((struct ufs_mtk_smc_arg) {__VA_ARGS__})

#define ufs_mtk_hie_cfg_smc(res, reg0, reg1, reg2) \
	ufs_mtk_old_smc(UFS_MTK_SIP_HIE_CFG_REQUEST, &(res), reg0, reg1, reg2, UFS_CRYPTO_SESSION_ID)
#endif

static inline void _ufs_mtk_smc(struct ufs_mtk_smc_arg s)
{
	arm_smccc_smc(MTK_SIP_UFS_CONTROL,
		s.cmd,
		s.v1, s.v2, s.v3, s.v4, s.v5, s.v6, s.res);
}

#define ufs_mtk_smc(...) \
	_ufs_mtk_smc((struct ufs_mtk_smc_arg) {__VA_ARGS__})

/* Sip kernel interface */
#define ufs_mtk_va09_pwr_ctrl(res, on) \
	ufs_mtk_smc(UFS_MTK_SIP_VA09_PWR_CTRL, &(res), on)

#define ufs_mtk_crypto_ctrl(res, enable) \
	ufs_mtk_smc(UFS_MTK_SIP_CRYPTO_CTRL, &(res), enable)

#define ufs_mtk_ref_clk_notify(on, stage, res) \
	ufs_mtk_smc(UFS_MTK_SIP_REF_CLK_NOTIFICATION, &(res), on, stage)

#define ufs_mtk_device_reset_ctrl(high, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_RESET, &(res), high)

#define ufs_mtk_sram_pwr_ctrl(on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_SRAM_PWR_CTRL, &(res), on)

#define ufs_mtk_get_vcc_num(res) \
	ufs_mtk_smc(UFS_MTK_SIP_GET_VCC_NUM, &(res))

#define ufs_mtk_device_pwr_ctrl(on, ufs_version, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_PWR_CTRL, &(res), on, ufs_version)

#define ufs_mtk_mphy_ctrl(op, res) \
	ufs_mtk_smc(UFS_MTK_SIP_MPHY_CTRL, &(res), op)

#define ufs_mtk_mtcmos_ctrl(op, res) \
	ufs_mtk_smc(UFS_MTK_SIP_MTCMOS_CTRL, &(res), op)

#define ufs_mtk_rpmb_key(region, pos, res) \
	ufs_mtk_smc(UFS_MTK_SIP_RPMB_KEY, &(res), region, pos)

#endif /* !_UFS_MEDIATEK_SIP_H */
