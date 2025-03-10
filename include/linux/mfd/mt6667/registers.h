/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MFD_MT6667_REGISTERS_H__
#define __MFD_MT6667_REGISTERS_H__

/* PMIC Registers */
/* PMIC interrupts */
#define MT6667_TOPSTATUS                                                (0x23)
#define MT6667_MISC_TOP_INT_CON0                                        (0x2a)
#define MT6667_MISC_TOP_INT_STATUS0                                     (0x36)
#define MT6667_TOP_INT_STATUS1                                          (0x41)
#define MT6667_PSC_TOP_INT_CON0                                         (0x90f)
#define MT6667_PSC_TOP_INT_STATUS0                                      (0x915)
#define MT6667_STRUP_CON11                                              (0xa16)
#define MT6667_PCHR_VREF_ELR_10                                         (0xa9f)
#define MT6667_HK_TOP_INT_CON0                                          (0xf91)
#define MT6667_HK_TOP_INT_STATUS0                                       (0xf97)
#define MT6667_BUCK_TOP_INT_CON0                                        (0x1415)
#define MT6667_BUCK_TOP_INT_STATUS0                                     (0x142d)

#define MT6667_CHRDET_DEB_ADDR                                          MT6667_TOPSTATUS
#define MT6667_CHRDET_DEB_MASK                                          (0x1)
#define MT6667_CHRDET_DEB_SHIFT                                         (2)
#define MT6667_RG_VBB_UVLO_VTHL_ADDR                                    MT6667_PCHR_VREF_ELR_10
#define MT6667_RG_VBB_UVLO_VTHL_MASK                                    (0xF)
#define MT6667_RG_VBB_UVLO_VTHL_SHIFT                                   (0)
#define MT6667_RG_VSYS_UVLO_VTHL_ADDR                                   MT6667_PCHR_VREF_ELR_10
#define MT6667_RG_VSYS_UVLO_VTHL_MASK                                   (0xF)
#define MT6667_RG_VSYS_UVLO_VTHL_SHIFT                                  (4)

/* PRE-OT RG */
#define MT6667_PMIC_RG_PRE_OT_COUNT_ADDR                                (0xb10)
#define MT6667_PMIC_RG_PRE_OT_COUNT_MASK                                (0xff)
#define MT6667_PMIC_RG_PRE_OT_COUNT_SHIFT                               (0)
#define MT6667_PMIC_RG_PRE_OT_COUNT_CLR_ADDR                            (0xb11)
#define MT6667_PMIC_RG_PRE_OT_COUNT_CLR_MASK                            (0x1)
#define MT6667_PMIC_RG_PRE_OT_COUNT_CLR_SHIFT                           (3)

/* CURRENT CLAMPING RG */
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_CLR_ADDR                         (0x144d)
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_CLR_MASK                         (0x1)
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_CLR_SHIFT                        (0)
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_EN_ADDR                          (0x144d)
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_EN_MASK                          (0x1)
#define MT6667_PMIC_RG_BUCK_OC_STS_CNT_EN_SHIFT                         (1)
#define MT6667_PMIC_RG_BUCK_TOP_RSV0_ADDR                               (0x144d)
#define MT6667_PMIC_RG_BUCK_TOP_RSV0_MASK                               (0x3f)
#define MT6667_PMIC_RG_BUCK_TOP_RSV0_SHIFT                              (2)
#define MT6667_PMIC_RG_BUCK1_OC_STS_CNT_ADDR                            (0x144e)
#define MT6667_PMIC_RG_BUCK1_OC_STS_CNT_MASK                            (0xff)
#define MT6667_PMIC_RG_BUCK1_OC_STS_CNT_SHIFT                           (0)
#define MT6667_PMIC_RG_BUCK2_OC_STS_CNT_ADDR                            (0x144f)
#define MT6667_PMIC_RG_BUCK2_OC_STS_CNT_MASK                            (0xff)
#define MT6667_PMIC_RG_BUCK2_OC_STS_CNT_SHIFT                           (0)
#define MT6667_PMIC_RG_BUCK3_OC_STS_CNT_ADDR                            (0x1450)
#define MT6667_PMIC_RG_BUCK3_OC_STS_CNT_MASK                            (0xff)
#define MT6667_PMIC_RG_BUCK3_OC_STS_CNT_SHIFT                           (0)
#define MT6667_PMIC_RG_BUCK4_OC_STS_CNT_ADDR                            (0x1451)
#define MT6667_PMIC_RG_BUCK4_OC_STS_CNT_MASK                            (0xff)
#define MT6667_PMIC_RG_BUCK4_OC_STS_CNT_SHIFT                           (0)

/* SPMI PMIC Debug Register */
#define MT6667_RG_SMT_SPMI_M_SCL_ADDR                                     (0xa4)
#define MT6667_RG_SMT_SPMI_M_SCL_MASK                                     (0x1)
#define MT6667_RG_SMT_SPMI_M_SCL_SHIFT                                    (4)
#define MT6667_RG_SMT_SPMI_M_SDA_ADDR                                     (0xa4)
#define MT6667_RG_SMT_SPMI_M_SDA_MASK                                     (0x1)
#define MT6667_RG_SMT_SPMI_M_SDA_SHIFT                                    (5)
#define MT6667_RG_OCTL_SPMI_M_SDA_ADDR                                    (0x14e)
#define MT6667_RG_OCTL_SPMI_M_SDA_MASK                                    (0x1f)
#define MT6667_RG_OCTL_SPMI_M_SDA_SHIFT                                   (2)
#define MT6667_GPIO_PULLEN0_4_ADDR                                        (0x14e)
#define MT6667_GPIO_PULLEN0_4_MASK                                        (0x1)
#define MT6667_GPIO_PULLEN0_4_SHIFT                                       (0)
#define MT6667_GPIO_PULLEN0_5_ADDR                                        (0x14e)
#define MT6667_GPIO_PULLEN0_5_MASK                                        (0x1)
#define MT6667_GPIO_PULLEN0_5_SHIFT                                       (1)
#define MT6667_RG_TOP_CFG_RSV1_0_ADDR                                     (0x151)
#define MT6667_RG_TOP_CFG_RSV1_0_MASK                                     (0x1)
#define MT6667_RG_TOP_CFG_RSV1_0_SHIFT                                    (4)
#define MT6667_RG_TOP_CFG_RSV1_1_ADDR                                     (0x151)
#define MT6667_RG_TOP_CFG_RSV1_1_MASK                                     (0x1)
#define MT6667_RG_TOP_CFG_RSV1_1_SHIFT                                    (5)
#define MT6667_GPIO_PULLSEL0_4_ADDR                                       (0x8e)
#define MT6667_GPIO_PULLSEL0_4_MASK                                       (0x1)
#define MT6667_GPIO_PULLSEL0_4_SHIFT                                      (4)
#define MT6667_GPIO_PULLSEL0_5_ADDR                                       (0x8e)
#define MT6667_GPIO_PULLSEL0_5_MASK                                       (0x1)
#define MT6667_GPIO_PULLSEL0_5_SHIFT                                      (5)
#define MT6667_SPMI_PMIC_DBG_RGS {                                        \
	"RG_SMT_SPMI_M_SCL",                                              \
	"RG_SMT_SPMI_M_SDA",                                              \
	"RG_OCTL_SPMI_M_SDA",                                             \
	"GPIO_PULLEN0_4",                                                 \
	"GPIO_PULLEN0_5",                                                 \
	"RG_TOP_CFG_RSV1_0",                                              \
	"RG_TOP_CFG_RSV1_1",                                              \
	"GPIO_PULLSEL0_4",                                                \
	"GPIO_PULLSEL0_5"                                                 \
}

/* SPMI PMIC dump rg data */
#define MT6667_RGS_NPKT_CCLP_ERR                                          (0x4cb)
#define MT6667_RGS_NPKT_CCLP_CLR                                          (0x4ca)

#endif /* __MFD_MT6667_REGISTERS_H__ */
