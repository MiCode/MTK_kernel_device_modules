/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_bcrm_apinfra_io_ctrl_ao;
extern unsigned long dcm_bcrm_apinfra_io_noc_ao;
extern unsigned long dcm_bcrm_apinfra_mem_intf_noc_ao;
extern unsigned long dcm_bcrm_apinfra_hash_ctrl_ao;
extern unsigned long dcm_bcrm_apinfra_mem_ctrl_ao;
extern unsigned long dcm_bcrm_apinfra_io_east_ao;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_pericfg_ao_reg_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(bcrm_APINFRA_IO_CTRL_AO)
#define bcrm_APINFRA_IO_CTRL_AO (dcm_bcrm_apinfra_io_ctrl_ao)
#endif /* !defined(bcrm_APINFRA_IO_CTRL_AO) */
#if !defined(bcrm_APINFRA_IO_NOC_AO)
#define bcrm_APINFRA_IO_NOC_AO (dcm_bcrm_apinfra_io_noc_ao)
#endif /* !defined(bcrm_APINFRA_IO_NOC_AO) */
#if !defined(bcrm_APINFRA_MEM_INTF_NOC_AO)
#define bcrm_APINFRA_MEM_INTF_NOC_AO (dcm_bcrm_apinfra_mem_intf_noc_ao)
#endif /* !defined(bcrm_APINFRA_MEM_INTF_NOC_AO) */
#if !defined(bcrm_APINFRA_HASH_CTRL_AO)
#define bcrm_APINFRA_HASH_CTRL_AO (dcm_bcrm_apinfra_hash_ctrl_ao)
#endif /* !defined(bcrm_APINFRA_HASH_CTRL_AO) */
#if !defined(bcrm_APINFRA_MEM_CTRL_AO)
#define bcrm_APINFRA_MEM_CTRL_AO (dcm_bcrm_apinfra_mem_ctrl_ao)
#endif /* !defined(bcrm_APINFRA_MEM_CTRL_AO) */
#if !defined(bcrm_APINFRA_IO_EAST_AO)
#define bcrm_APINFRA_IO_EAST_AO (dcm_bcrm_apinfra_io_east_ao)
#endif /* !defined(bcrm_APINFRA_IO_EAST_AO) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(PERICFG_AO_REG_BASE)
#define PERICFG_AO_REG_BASE (dcm_pericfg_ao_reg_base)
#endif /* !defined(PERICFG_AO_REG_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef bcrm_APINFRA_IO_CTRL_AO
#undef bcrm_APINFRA_IO_NOC_AO
#undef bcrm_APINFRA_MEM_INTF_NOC_AO
#undef bcrm_APINFRA_HASH_CTRL_AO
#undef bcrm_APINFRA_MEM_CTRL_AO
#undef bcrm_APINFRA_IO_EAST_AO
#undef PERI_AO_BCRM_BASE
#undef PERICFG_AO_REG_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE         0x5000000
#define bcrm_APINFRA_IO_CTRL_AO      0x10156000
#define bcrm_APINFRA_IO_NOC_AO       0x14012000
#define bcrm_APINFRA_MEM_INTF_NOC_AO 0x14032000
#define bcrm_APINFRA_HASH_CTRL_AO    0x14091000
#define bcrm_APINFRA_MEM_CTRL_AO     0x14124000
#define bcrm_APINFRA_IO_EAST_AO      0x1431d000
#define PERI_AO_BCRM_BASE            0x16610000
#define PERICFG_AO_REG_BASE          0x16640000
#define VLP_AO_BCRM_BASE             0x1c030000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define CLUSTER0_MCUSYS_MST_VLP_L3_SHARE_DCM_CTRL                                        (MCUSYS_PAR_WRAP_BASE + 0xe00f8)
#define CLUSTER0_MCUSYS_MST_VLP_DSU_DCM_CFG                                              (MCUSYS_PAR_WRAP_BASE + 0xe02a8)
#define CLUSTER0_MCUSYS_MST_VLP_MPSYS_DCM_CFG                                            (MCUSYS_PAR_WRAP_BASE + 0xe02ac)
#define CLUSTER0_MCUSYS_MST_VLP_MCUSYS_SLV_VPS_DCM_CFG                                   (MCUSYS_PAR_WRAP_BASE + 0xe02b4)
#define CLUSTER0_MCUSYS_MST_VLP_MCUSYS_MST_VPS_AO_DCM_CFG                                (MCUSYS_PAR_WRAP_BASE + 0xe02b8)
#define CLUSTER0_MCUSYS_MST_VLP_MCUSYS_MST_VPS_DCM_CFG                                   (MCUSYS_PAR_WRAP_BASE + 0xe02bc)
#define CLUSTER0_MCUSYS_MST_VLP_MCUSYS_SLV_VC_DCM_CFG                                    (MCUSYS_PAR_WRAP_BASE + 0xe02b0)
#define CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG0                                             (MCUSYS_PAR_WRAP_BASE + 0xe02c0)
#define CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG1                                             (MCUSYS_PAR_WRAP_BASE + 0xe02c4)
#define CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG2                                             (MCUSYS_PAR_WRAP_BASE + 0xe02c8)
#define CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG3                                             (MCUSYS_PAR_WRAP_BASE + 0xe02cc)
#define CLUSTER0_MCUSYS_MST_VLP_QDCM_CONFIG4                                             (MCUSYS_PAR_WRAP_BASE + 0xe02d0)
#define CLUSTER0_MCUSYS_MST_VLP_GIC_SPI_SLOW_CK_CFG                                      (MCUSYS_PAR_WRAP_BASE + 0xe02e4)
#define CLK_AXI_VDNR_DCM_TOP_APINFRA_IO_INTX_BUS_u_APINFRA_IO_INTX_BUS_CTRL_0            (bcrm_APINFRA_IO_CTRL_AO + 0x8)
#define CLK_IO_NOC_VDNR_DCM_TOP_APINFRA_IO_INTF_PAR_BUS_u_APINFRA_IO_INTF_PAR_BUS_CTRL_0 (bcrm_APINFRA_IO_NOC_AO + 0x4)
#define VDNR_DCM_TOP_APINFRA_MEM_INTF_PAR_BUS_u_APINFRA_MEM_INTF_PAR_BUS_CTRL_0          (bcrm_APINFRA_MEM_INTF_NOC_AO + 0x0)
#define CLK_FHASH_CFG_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0  (bcrm_APINFRA_HASH_CTRL_AO + 0x10)
#define CLK_FHASH_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0      (bcrm_APINFRA_HASH_CTRL_AO + 0x18)
#define CLK_FHASH_NOC_VDNR_DCM_TOP_APINFRA_HASH_INTX_BUS_u_APINFRA_HASH_INTX_BUS_CTRL_0  (bcrm_APINFRA_HASH_CTRL_AO + 0x24)
#define CLK_FMEM_SUB_CFG_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0 (bcrm_APINFRA_MEM_CTRL_AO + 0xc)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0     (bcrm_APINFRA_MEM_CTRL_AO + 0x14)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_1     (bcrm_APINFRA_MEM_CTRL_AO + 0x18)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_2     (bcrm_APINFRA_MEM_CTRL_AO + 0x1c)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_3     (bcrm_APINFRA_MEM_CTRL_AO + 0x20)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_4     (bcrm_APINFRA_MEM_CTRL_AO + 0x24)
#define BCRM_APINFRA_IO_EAST_AO_APINFRA_IO_EST_FAXI_OFF_REG0                             (bcrm_APINFRA_IO_EAST_AO + 0x10)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0                                  (PERI_AO_BCRM_BASE + 0x44)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1                                  (PERI_AO_BCRM_BASE + 0x48)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2                                  (PERI_AO_BCRM_BASE + 0x4c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_3                                  (PERI_AO_BCRM_BASE + 0x50)
#define PERICFG_AO_REG_PERI_MASK_IDLE_TO_CKSYS                                           (PERICFG_AO_REG_BASE + 0x270)
#define VDNR_DCM_TOP_VLP_PAR_BUS_TOP_u_VLP_PAR_BUS_TOP_CTRL_0                            (VLP_AO_BCRM_BASE + 0x7c)

void dcm_mcusys_par_wrap_mcu_l3c_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_l3c_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_vdnr_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_vdnr_dcm_is_on(void);
void dcm_mcusys_par_wrap_dsu_bus_dcm(int on);
bool dcm_mcusys_par_wrap_dsu_bus_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cdb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_cdb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_slv_vc_bus_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_slv_vc_bus_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_slv_vps_bus_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_slv_vps_bus_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dbgnw_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_dbgnw_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_mst_vps_bus_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_mst_vps_bus_dcm_is_on(void);
void dcm_mcusys_par_wrap_mpsys_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mpsys_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mpsys_dbg_qdcm(int on);
bool dcm_mcusys_par_wrap_mpsys_dbg_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_gic_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_gic_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dsu_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_dsu_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_gic_spi_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_gic_spi_dcm_is_on(void);
void dcm_bcrm_apinfra_io_ctrl_ao_apinfra_faxi_off(int on);
bool dcm_bcrm_apinfra_io_ctrl_ao_apinfra_faxi_off_is_on(void);
void dcm_bcrm_apinfra_io_noc_ao_apinfra_io_noc_off(int on);
bool dcm_bcrm_apinfra_io_noc_ao_apinfra_io_noc_off_is_on(void);
void dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_hash_ctrl_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_hash_ctrl_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_io_east_ao_apinfra_io_est_faxi_off(int on);
bool dcm_bcrm_apinfra_io_east_ao_apinfra_io_est_faxi_off_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_pericfg_ao_reg_peri_bus_dcm(int on);
bool dcm_pericfg_ao_reg_peri_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
#endif /* __MTK_DCM_AUTOGEN_H__ */
