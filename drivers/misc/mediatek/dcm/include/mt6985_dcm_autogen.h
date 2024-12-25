/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_mcusys_cpc_base;
extern unsigned long dcm_mcusys_par_wrap_complex0_base;
extern unsigned long dcm_mcusys_par_wrap_complex1_base;
extern unsigned long dcm_ifrbus_ao_base;
extern unsigned long dcm_ifrrsi_base;
extern unsigned long dcm_ifriommu_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_ufs0_ao_bcrm_base;
extern unsigned long dcm_pcie0_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(MCUSYS_CPC_BASE)
#define MCUSYS_CPC_BASE (dcm_mcusys_cpc_base)
#endif /* !defined(MCUSYS_CPC_BASE) */
#if !defined(MCUSYS_PAR_WRAP_COMPLEX0_BASE)
#define MCUSYS_PAR_WRAP_COMPLEX0_BASE (dcm_mcusys_par_wrap_complex0_base)
#endif /* !defined(MCUSYS_PAR_WRAP_COMPLEX0_BASE) */
#if !defined(MCUSYS_PAR_WRAP_COMPLEX1_BASE)
#define MCUSYS_PAR_WRAP_COMPLEX1_BASE (dcm_mcusys_par_wrap_complex1_base)
#endif /* !defined(MCUSYS_PAR_WRAP_COMPLEX1_BASE) */
#if !defined(IFRBUS_AO_BASE)
#define IFRBUS_AO_BASE (dcm_ifrbus_ao_base)
#endif /* !defined(IFRBUS_AO_BASE) */
#if !defined(IFRRSI_BASE)
#define IFRRSI_BASE (dcm_ifrrsi_base)
#endif /* !defined(IFRRSI_BASE) */
#if !defined(IFRIOMMU_BASE)
#define IFRIOMMU_BASE (dcm_ifriommu_base)
#endif /* !defined(IFRIOMMU_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(UFS0_AO_BCRM_BASE)
#define UFS0_AO_BCRM_BASE (dcm_ufs0_ao_bcrm_base)
#endif /* !defined(UFS0_AO_BCRM_BASE) */
#if !defined(PCIE0_AO_BCRM_BASE)
#define PCIE0_AO_BCRM_BASE (dcm_pcie0_ao_bcrm_base)
#endif /* !defined(PCIE0_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef MCUSYS_CPC_BASE
#undef MCUSYS_PAR_WRAP_COMPLEX0_BASE
#undef MCUSYS_PAR_WRAP_COMPLEX1_BASE
#undef IFRBUS_AO_BASE
#undef IFRRSI_BASE
#undef IFRIOMMU_BASE
#undef PERI_AO_BCRM_BASE
#undef UFS0_AO_BCRM_BASE
#undef PCIE0_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE          0xc000200
#define MCUSYS_CPC_BASE               0xc040000
#define MCUSYS_PAR_WRAP_COMPLEX0_BASE 0xc18c000
#define MCUSYS_PAR_WRAP_COMPLEX1_BASE 0xc1ac000
#define IFRBUS_AO_BASE                0x1002c000
#define IFRRSI_BASE                   0x10324000
#define IFRIOMMU_BASE                 0x10330000
#define PERI_AO_BCRM_BASE             0x11035000
#define UFS0_AO_BCRM_BASE             0x112ba000
#define PCIE0_AO_BCRM_BASE            0x112e2000
#define VLP_AO_BCRM_BASE              0x1c017000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define TOPCKGEN_MMU_DCM_DIS                             (IFRIOMMU_BASE + 0x50)
#define TOPCKGEN_RSI_DCM_CON                             (IFRRSI_BASE + 0x4)
#define DCM_SET_RW_0                                     (IFRBUS_AO_BASE + 0xb00)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0  (PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1  (PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2  (PERI_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_0            (UFS0_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_UFS_BUS_u_UFS_BUS_CTRL_1            (UFS0_AO_BCRM_BASE + 0x24)
#define VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_0        (PCIE0_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_PEXTP_BUS_u_PEXTP_BUS_CTRL_1        (PCIE0_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0    (VLP_AO_BCRM_BASE + 0xa4)
#define MCUSYS_PAR_WRAP_CPC_DCM_Enable                   (MCUSYS_CPC_BASE + 0x19c)
#define MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0                  (MCUSYS_PAR_WRAP_BASE + 0x70)
#define MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN                  (MCUSYS_PAR_WRAP_BASE + 0x78)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG0                     (MCUSYS_PAR_WRAP_BASE + 0x7c)
#define MCUSYS_PAR_WRAP_CI700_DCM_CTRL                   (MCUSYS_PAR_WRAP_BASE + 0x98)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG0                     (MCUSYS_PAR_WRAP_BASE + 0x80)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG1                     (MCUSYS_PAR_WRAP_BASE + 0x84)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa4)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xa8)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xac)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG          (MCUSYS_PAR_WRAP_BASE + 0xb0)
#define MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0                 (MCUSYS_PAR_WRAP_BASE + 0xb4)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG2                     (MCUSYS_PAR_WRAP_BASE + 0x88)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG3                     (MCUSYS_PAR_WRAP_BASE + 0x8c)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG1                     (MCUSYS_PAR_WRAP_BASE + 0x9c)
#define MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG             (MCUSYS_PAR_WRAP_BASE + 0x94)
#define MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG (MCUSYS_PAR_WRAP_BASE + 0xb8)
#define MCUSYS_PAR_WRAP_COMPLEX0_STALL_DCM_CONF          (MCUSYS_PAR_WRAP_COMPLEX0_BASE + 0x210)
#define MCUSYS_PAR_WRAP_COMPLEX1_STALL_DCM_CONF          (MCUSYS_PAR_WRAP_COMPLEX1_BASE + 0x210)

bool dcm_topckgen_infra_iommu_dcm_is_on(void);
void dcm_topckgen_infra_iommu_dcm(int on);
bool dcm_topckgen_infra_rsi_dcm_is_on(void);
void dcm_topckgen_infra_rsi_dcm(int on);
bool dcm_ifrbus_ao_infra_bus_dcm_is_on(void);
void dcm_ifrbus_ao_infra_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_ufs0_ao_bcrm_ufs_bus_dcm_is_on(void);
void dcm_ufs0_ao_bcrm_ufs_bus_dcm(int on);
bool dcm_pcie0_ao_bcrm_pextp_bus_dcm_is_on(void);
void dcm_pcie0_ao_bcrm_pextp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_pbi_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_turbo_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_dsu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dsu_stalldcm(int on);
bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_io_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_misc_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_stalldcm(int on);
#endif /* __MTK_DCM_AUTOGEN_H__ */
