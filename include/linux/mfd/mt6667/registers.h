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

#endif /* __MFD_MT6667_REGISTERS_H__ */
