/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __LINUX_REGULATOR_MT6667_H
#define __LINUX_REGULATOR_MT6667_H

enum MT6667_regulator_id {
	MT6667_ID_BUCK1,
	MT6667_ID_BUCK2,
	MT6667_ID_BUCK3,
	MT6667_ID_BUCK4,
        MT6667_MAX_REGULATOR,
};

#define MTK_REGULATOR_MAX_NR MT6667_MAX_REGULATOR

/* Register */
#define MT6667_TOP_TMA_KEY_L                        (0x3b4)
#define MT6667_CPSWKEY                              (0xa25)
#define MT6667_CPSWKEY_H                            (0xa26)
#define MT6667_BUCK_TOP_KEY_PROT_LO                 (0x1442)
#define MT6667_BUCK1_OP_EN_1                        (0x14a4)
#define MT6667_BUCK1_OP_EN_1_SET                    (0x14a5)
#define MT6667_BUCK1_OP_EN_1_CLR                    (0x14a6)
#define MT6667_BUCK2_OP_EN_1                        (0x1524)
#define MT6667_BUCK2_OP_EN_1_SET                    (0x1525)
#define MT6667_BUCK2_OP_EN_1_CLR                    (0x1526)
#define MT6667_BUCK3_OP_EN_1                        (0x15a4)
#define MT6667_BUCK3_OP_EN_1_SET                    (0x15a5)
#define MT6667_BUCK3_OP_EN_1_CLR                    (0x15a6)
#define MT6667_BUCK4_OP_EN_1                        (0x1624)
#define MT6667_BUCK4_OP_EN_1_SET                    (0x1625)
#define MT6667_BUCK4_OP_EN_1_CLR                    (0x1626)

#define MT6667_PMIC_RG_BUCK1_VOSEL_ADDR             (0x240)
#define MT6667_PMIC_RG_BUCK1_VOSEL_MASK             (0xff)
#define MT6667_PMIC_RG_BUCK2_VOSEL_ADDR             (0x241)
#define MT6667_PMIC_RG_BUCK2_VOSEL_MASK             (0xff)
#define MT6667_PMIC_RG_BUCK3_VOSEL_ADDR             (0x242)
#define MT6667_PMIC_RG_BUCK3_VOSEL_MASK             (0xff)
#define MT6667_PMIC_RG_BUCK4_VOSEL_ADDR             (0x243)
#define MT6667_PMIC_RG_BUCK4_VOSEL_MASK             (0xff)
#define MT6667_PMIC_RG_BUCK1_EN_ADDR                (0x1487)
#define MT6667_PMIC_RG_BUCK1_EN_SHIFT               (0)
#define MT6667_PMIC_RG_BUCK1_LP_ADDR                (0x1487)
#define MT6667_PMIC_RG_BUCK1_LP_SHIFT               (1)
#define MT6667_PMIC_DA_BUCK1_EN_ADDR                (0x14c5)
#define MT6667_PMIC_RG_BUCK2_EN_ADDR                (0x1507)
#define MT6667_PMIC_RG_BUCK2_EN_SHIFT               (0)
#define MT6667_PMIC_RG_BUCK2_LP_ADDR                (0x1507)
#define MT6667_PMIC_RG_BUCK2_LP_SHIFT               (1)
#define MT6667_PMIC_DA_BUCK2_EN_ADDR                (0x1545)
#define MT6667_PMIC_RG_BUCK3_EN_ADDR                (0x1587)
#define MT6667_PMIC_RG_BUCK3_EN_SHIFT               (0)
#define MT6667_PMIC_RG_BUCK3_LP_ADDR                (0x1587)
#define MT6667_PMIC_RG_BUCK3_LP_SHIFT               (1)
#define MT6667_PMIC_DA_BUCK3_EN_ADDR                (0x15c5)
#define MT6667_PMIC_RG_BUCK4_EN_ADDR                (0x1607)
#define MT6667_PMIC_RG_BUCK4_EN_SHIFT               (0)
#define MT6667_PMIC_RG_BUCK4_LP_ADDR                (0x1607)
#define MT6667_PMIC_RG_BUCK4_LP_SHIFT               (1)
#define MT6667_PMIC_DA_BUCK4_EN_ADDR                (0x1645)
#define MT6667_PMIC_RG_FCCM_PH1_ADDR                (0x1990)
#define MT6667_PMIC_RG_FCCM_PH1_SHIFT               (0)
#define MT6667_PMIC_RG_FCCM_PH2_ADDR                (0x1990)
#define MT6667_PMIC_RG_FCCM_PH2_SHIFT               (1)
#define MT6667_PMIC_RG_FCCM_PH3_ADDR                (0x1990)
#define MT6667_PMIC_RG_FCCM_PH3_SHIFT               (2)
#define MT6667_PMIC_RG_FCCM_PH4_ADDR                (0x1990)
#define MT6667_PMIC_RG_FCCM_PH4_SHIFT               (3)

#endif /* __LINUX_REGULATOR_MT6667_H */