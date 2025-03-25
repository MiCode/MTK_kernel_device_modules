// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/spinlock.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_pcm_def.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_trace_event.h>
#include <lpm_dbg_logger.h>
#include <spm_reg.h>
#include <pwr_ctrl.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
#include <lpm_sys_res.h>
#endif

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define SPM_HW_CG_CHECK_MASK_0 (0x1)
#define SPM_HW_CG_CHECK_MASK_1 (0x7f)
#define SPM_HW_CG_CHECK_SHIFT_0 (31)

#define SPM_REQ_STA_NUM (((SPM_REQ_STA_17 - SPM_REQ_STA_0) / 4) + 1)
#define SPM_DATA_INVALID_MAGIC (0xdeadbeef)

#define plat_mmio_read(offset)	__raw_readl(lpm_spm_base + offset)

const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER_B",
	[1] = " R12_TWAM_PMSR_DVFSRC",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT_EVENT_B",
	[5] = " R12_CONN2AP_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_CCIF1_EVENT_B",
	[10] = " R12_SSPM2SPM_WAKEUP_B",
	[11] = " R12_SCP2SPM_WAKEUP_B",
	[12] = " R12_ADSP2SPM_WAKEUP_B",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_UART_EVENT_B",
	[17] = " R12_AUDIO_IRQ_SOUNDWIRE_ADDA_B",
	[18] = " R12_SYSTIMER_EVENT_B",
	[19] = " R12_EINT_EVENT_SECURE_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_PBUS2SPM_IRQ_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD_WDT_B",
	[26] = " R12_AP2AP_PEER_WAKEUP_B",
	[27] = " R12_SEJ_B",
	[28] = " R12_CPU_WAKEUP",
	[29] = " R12_APUSYS_WAKE_HOST",
	[30] = " R12_PCIE_WAKE_B",
	[31] = " R12_GIP2SPM_M_WAKE",
};

const char *wakesrc_h_str[32] = {
	[0] = " R12_H_GIP2SPM_P_WAKEUP",
	[1] = " R12_H_ERROR_FLAG_WDT_IRQ_B",
	[2] = " R12_H_SPM_ACK_CHK_WAKEUP",
	[3] = " R12_H_OCLA2SPM_IRQ_B",
	[4] = " R12_H_VLP_HWCCF_IRQ_B",
	[5] = " R12_H_MM_HWCCF_IRQ_B",
	[6] = " R12_RESERVED_BIT6",
	[7] = " R12_H_RESERVED_BIT7",
	[8] = " R12_H_RESERVED_BIT8",
	[9] = " R12_H_RESERVED_BIT9",
	[10] = " R12_H_RESERVED_BIT10",
	[11] = " R12_H_RESERVED_BIT11",
	[12] = " R12_H_RESERVED_BIT12",
	[13] = " R12_H_RESERVED_BIT13",
	[14] = " R12_H_RESERVED_BIT14",
	[15] = " R12_H_RESERVED_BIT15",
	[16] = " R12_H_RESERVED_BIT16",
	[17] = " R12_H_RESERVED_BIT17",
	[18] = " R12_H_RESERVED_BIT18",
	[19] = " R12_H_RESERVED_BIT19",
	[20] = " R12_H_RESERVED_BIT20",
	[21] = " R12_H_RESERVED_BIT21",
	[22] = " R12_H_RESERVED_BIT22",
	[23] = " R12_H_RESERVED_BIT23",
	[24] = " R12_H_RESERVED_BIT24",
	[25] = " R12_H_RESERVED_BIT25",
	[26] = " R12_H_RESERVED_BIT26",
	[27] = " R12_H_RESERVED_BIT27",
	[28] = " R12_H_RESERVED_BIT28",
	[29] = " R12_H_RESERVED_BIT29",
	[30] = " R12_H_RESERVED_BIT30",
	[31] = " R12_H_RESERVED_BIT31",
};



static char *pwr_ctrl_str[PW_MAX_COUNT] = {
	[PW_PCM_FLAGS] = "pcm_flags",
	[PW_PCM_FLAGS_CUST] = "pcm_flags_cust",
	[PW_PCM_FLAGS_CUST_SET] = "pcm_flags_cust_set",
	[PW_PCM_FLAGS_CUST_CLR] = "pcm_flags_cust_clr",
	[PW_PCM_FLAGS1] = "pcm_flags1",
	[PW_PCM_FLAGS1_CUST] = "pcm_flags1_cust",
	[PW_PCM_FLAGS1_CUST_SET] = "pcm_flags1_cust_set",
	[PW_PCM_FLAGS1_CUST_CLR] = "pcm_flags1_cust_clr",
	[PW_TIMER_VAL] = "timer_val",
	[PW_TIMER_VAL_CUST] = "timer_val_cust",
	[PW_TIMER_VAL_RAMP_EN] = "timer_val_ramp_en",
	[PW_TIMER_VAL_RAMP_EN_SEC] = "timer_val_ramp_en_sec",
	[PW_WAKE_SRC] = "wake_src",
	[PW_WAKE_SRC_H] = "wake_src_h",
	[PW_WAKE_SRC_CUST] = "wake_src_cust",
	[PW_WAKE_SRC_H_CUST] = "wake_src_h_cust",
	[PW_WAKELOCK_TIMER_VAL] = "wakelock_timer_val",
	[PW_WDT_DISABLE] = "wdt_disable",

	/* SPM_SRC_REQ_0 */
	[PW_REG_SPM_ADSP_DPSW_REQ] = "reg_spm_adsp_dpsw_req",
	[PW_REG_SPM_ADSP_MAILBOX_REQ] = "reg_spm_adsp_mailbox_req",
	[PW_REG_SPM_AOV_REQ] = "reg_spm_aov_req",
	[PW_REG_SPM_APSRC_REQ] = "reg_spm_apsrc_req",
	[PW_REG_SPM_CHIFR_DATA_COH_REQ] = "reg_spm_chifr_data_coh_req",
	[PW_REG_SPM_CHIFR_TCU_COH_REQ] = "reg_spm_chifr_tcu_coh_req",
	[PW_REG_SPM_DCS_REQ] = "reg_spm_dcs_req",
	[PW_REG_SPM_DVFS_REQ] = "reg_spm_dvfs_req",
	[PW_REG_SPM_EMI_REQ] = "reg_spm_emi_req",
	[PW_REG_SPM_F26M_REQ] = "reg_spm_f26m_req",
	[PW_REG_SPM_INFRA_REQ] = "reg_spm_infra_req",
	[PW_REG_SPM_PMIC_REQ] = "reg_spm_pmic_req",
	[PW_REG_SPM_SCP_MAILBOX_REQ] = "reg_spm_scp_mailbox_req",
	[PW_REG_SPM_SSPM_MAILBOX_REQ] = "reg_spm_sspm_mailbox_req",
	[PW_REG_SPM_SW_MAILBOX_REQ] = "reg_spm_sw_mailbox_req",
	[PW_REG_SPM_SYSCO_REQ] = "reg_spm_sysco_req",

	/* SPM_SRC_REQ_1 */
	[PW_REG_SPM_VCORE_REQ] = "reg_spm_vcore_req",
	[PW_REG_SPM_VRF18_REQ] = "reg_spm_vrf18_req",
	[PW_REG_SPM_MCU_MEM_REQ] = "reg_spm_mcu_mem_req",

	/* SPM_SRC_MASK_0 */
	[PW_REG_ADSP_ADSP_DPSW_REQ_MASK_B] = "reg_adsp_adsp_dpsw_req_mask_b",
	[PW_REG_ADSP_APSRC_REQ_MASK_B] = "reg_adsp_apsrc_req_mask_b",
	[PW_REG_ADSP_EMI_REQ_MASK_B] = "reg_adsp_emi_req_mask_b",
	[PW_REG_ADSP_INFRA_REQ_MASK_B] = "reg_adsp_infra_req_mask_b",
	[PW_REG_ADSP_PMIC_REQ_MASK_B] = "reg_adsp_pmic_req_mask_b",
	[PW_REG_ADSP_SRCCLKENA_MASK_B] = "reg_adsp_srcclkena_mask_b",
	[PW_REG_ADSP_VCORE_REQ_MASK_B] = "reg_adsp_vcore_req_mask_b",
	[PW_REG_ADSP_VRF18_REQ_MASK_B] = "reg_adsp_vrf18_req_mask_b",
	[PW_REG_APIFR_HASH_APSRC_REQ_MASK_B] = "reg_apifr_hash_apsrc_req_mask_b",
	[PW_REG_APIFR_HASH_EMI_REQ_MASK_B] = "reg_apifr_hash_emi_req_mask_b",
	[PW_REG_APIFR_HASH_INFRA_REQ_MASK_B] = "reg_apifr_hash_infra_req_mask_b",
	[PW_REG_APIFR_HASH_PMIC_REQ_MASK_B] = "reg_apifr_hash_pmic_req_mask_b",
	[PW_REG_APIFR_HASH_SRCCLKENA_MASK_B] = "reg_apifr_hash_srcclkena_mask_b",
	[PW_REG_APIFR_HASH_VCORE_REQ_MASK_B] = "reg_apifr_hash_vcore_req_mask_b",
	[PW_REG_APIFR_HASH_VRF18_REQ_MASK_B] = "reg_apifr_hash_vrf18_req_mask_b",
	[PW_REG_APIFR_IO_APSRC_REQ_MASK_B] = "reg_apifr_io_apsrc_req_mask_b",
	[PW_REG_APIFR_IO_EMI_REQ_MASK_B] = "reg_apifr_io_emi_req_mask_b",
	[PW_REG_APIFR_IO_INFRA_REQ_MASK_B] = "reg_apifr_io_infra_req_mask_b",
	[PW_REG_APIFR_IO_PMIC_REQ_MASK_B] = "reg_apifr_io_pmic_req_mask_b",
	[PW_REG_APIFR_IO_VCORE_REQ_MASK_B] = "reg_apifr_io_vcore_req_mask_b",
	[PW_REG_APIFR_IO_VRF18_REQ_MASK_B] = "reg_apifr_io_vrf18_req_mask_b",
	[PW_REG_APIFR_MEM_APSRC_REQ_MASK_B] = "reg_apifr_mem_apsrc_req_mask_b",
	[PW_REG_APIFR_MEM_EMI_REQ_MASK_B] = "reg_apifr_mem_emi_req_mask_b",
	[PW_REG_APIFR_MEM_INFRA_REQ_MASK_B] = "reg_apifr_mem_infra_req_mask_b",
	[PW_REG_APIFR_MEM_PMIC_REQ_MASK_B] = "reg_apifr_mem_pmic_req_mask_b",
	[PW_REG_APIFR_MEM_SRCCLKENA_MASK_B] = "reg_apifr_mem_srcclkena_mask_b",
	[PW_REG_APIFR_MEM_VCORE_REQ_MASK_B] = "reg_apifr_mem_vcore_req_mask_b",
	[PW_REG_APIFR_MEM_VRF18_REQ_MASK_B] = "reg_apifr_mem_vrf18_req_mask_b",
	[PW_REG_APU_AOV_REQ_MASK_B] = "reg_apu_aov_req_mask_b",
	[PW_REG_APU_APSRC_REQ_MASK_B] = "reg_apu_apsrc_req_mask_b",
	[PW_REG_APU_CHIFR_DATA_COH_REQ_MASK_B] = "reg_apu_chifr_data_coh_req_mask_b",
	[PW_REG_APU_CHIFR_TCU_COH_REQ_MASK_B] = "reg_apu_chifr_tcu_coh_req_mask_b",

	/* SPM_SRC_MASK_1 */
	[PW_REG_APU_EMI_REQ_MASK_B] = "reg_apu_emi_req_mask_b",
	[PW_REG_APU_INFRA_REQ_MASK_B] = "reg_apu_infra_req_mask_b",
	[PW_REG_APU_PMIC_REQ_MASK_B] = "reg_apu_pmic_req_mask_b",
	[PW_REG_APU_SRCCLKENA_MASK_B] = "reg_apu_srcclkena_mask_b",
	[PW_REG_APU_VCORE_REQ_MASK_B] = "reg_apu_vcore_req_mask_b",
	[PW_REG_APU_VRF18_REQ_MASK_B] = "reg_apu_vrf18_req_mask_b",
	[PW_REG_AUDIO_APSRC_REQ_MASK_B] = "reg_audio_apsrc_req_mask_b",
	[PW_REG_AUDIO_INFRA_REQ_MASK_B] = "reg_audio_infra_req_mask_b",
	[PW_REG_AUDIO_PMIC_REQ_MASK_B] = "reg_audio_pmic_req_mask_b",
	[PW_REG_AUDIO_SRCCLKENA_MASK_B] = "reg_audio_srcclkena_mask_b",
	[PW_REG_AUDIO_VCORE_REQ_MASK_B] = "reg_audio_vcore_req_mask_b",
	[PW_REG_AUDIO_VRF18_REQ_MASK_B] = "reg_audio_vrf18_req_mask_b",
	[PW_REG_CCIF_APSRC_REQ_MASK_B] = "reg_ccif_apsrc_req_mask_b",

	/* SPM_SRC_MASK_2 */
	[PW_REG_CCIF_INFRA_REQ_MASK_B] = "reg_ccif_infra_req_mask_b",
	[PW_REG_CCIF_SRCCLKENA_MASK_B] = "reg_ccif_srcclkena_mask_b",
	[PW_REG_CPU_SMMU_SYSCO_REQ_MASK_B] = "reg_cpu_smmu_sysco_req_mask_b",
	[PW_REG_MCU_MCU_MEM_REQ_MASK_B] = "reg_mcu_mcu_mem_req_mask_b",

	/* SPM_SRC_MASK_3 */
	[PW_REG_CCIF_VCORE_REQ_MASK_B] = "reg_ccif_vcore_req_mask_b",
	[PW_REG_CG_CHECK_APSRC_REQ_MASK_B] = "reg_cg_check_apsrc_req_mask_b",
	[PW_REG_CG_CHECK_EMI_REQ_MASK_B] = "reg_cg_check_emi_req_mask_b",
	[PW_REG_CG_CHECK_INFRA_REQ_MASK_B] = "reg_cg_check_infra_req_mask_b",
	[PW_REG_CG_CHECK_PMIC_REQ_MASK_B] = "reg_cg_check_pmic_req_mask_b",
	[PW_REG_CG_CHECK_SRCCLKENA_MASK_B] = "reg_cg_check_srcclkena_mask_b",
	[PW_REG_CG_CHECK_VCORE_REQ_MASK_B] = "reg_cg_check_vcore_req_mask_b",
	[PW_REG_CG_CHECK_VRF18_REQ_MASK_B] = "reg_cg_check_vrf18_req_mask_b",
	[PW_REG_CKSYS_SRCCLKENA_MASK_B] = "reg_cksys_srcclkena_mask_b",
	[PW_REG_CKSYS1_PMIC_REQ_MASK_B] = "reg_cksys1_pmic_req_mask_b",
	[PW_REG_CKSYS1_SRCCLKENA_MASK_B] = "reg_cksys1_srcclkena_mask_b",
	[PW_REG_CKSYS1_VCORE_REQ_MASK_B] = "reg_cksys1_vcore_req_mask_b",
	[PW_REG_CKSYS2_PMIC_REQ_MASK_B] = "reg_cksys2_pmic_req_mask_b",
	[PW_REG_CKSYS2_SRCCLKENA_MASK_B] = "reg_cksys2_srcclkena_mask_b",
	[PW_REG_CKSYS2_VCORE_REQ_MASK_B] = "reg_cksys2_vcore_req_mask_b",
	[PW_REG_CONN_APSRC_REQ_MASK_B] = "reg_conn_apsrc_req_mask_b",
	[PW_REG_CONN_EMI_REQ_MASK_B] = "reg_conn_emi_req_mask_b",
	[PW_REG_CONN_INFRA_REQ_MASK_B] = "reg_conn_infra_req_mask_b",
	[PW_REG_CONN_PMIC_REQ_MASK_B] = "reg_conn_pmic_req_mask_b",
	[PW_REG_CONN_SRCCLKENA_MASK_B] = "reg_conn_srcclkena_mask_b",
	[PW_REG_CONN_SRCCLKENB_MASK_B] = "reg_conn_srcclkenb_mask_b",

	/* SPM_SRC_MASK_4 */
	[PW_REG_CONN_VCORE_REQ_MASK_B] = "reg_conn_vcore_req_mask_b",
	[PW_REG_CONN_VRF18_REQ_MASK_B] = "reg_conn_vrf18_req_mask_b",
	[PW_REG_MCUPM_APSRC_REQ_MASK_B] = "reg_mcupm_apsrc_req_mask_b",
	[PW_REG_MCUPM_EMI_REQ_MASK_B] = "reg_mcupm_emi_req_mask_b",
	[PW_REG_MCUPM_INFRA_REQ_MASK_B] = "reg_mcupm_infra_req_mask_b",
	[PW_REG_MCUPM_PMIC_REQ_MASK_B] = "reg_mcupm_pmic_req_mask_b",
	[PW_REG_MCUPM_SRCCLKENA_MASK_B] = "reg_mcupm_srcclkena_mask_b",
	[PW_REG_MCUPM_VCORE_REQ_MASK_B] = "reg_mcupm_vcore_req_mask_b",
	[PW_REG_MCUPM_VRF18_REQ_MASK_B] = "reg_mcupm_vrf18_req_mask_b",
	[PW_REG_DPM_APSRC_REQ_MASK_B] = "reg_dpm_apsrc_req_mask_b",
	[PW_REG_DPM_DCS_REQ_MASK_B] = "reg_dpm_dcs_req_mask_b",
	[PW_REG_DPM_EMI_REQ_MASK_B] = "reg_dpm_emi_req_mask_b",
	[PW_REG_DPM_INFRA_REQ_MASK_B] = "reg_dpm_infra_req_mask_b",
	[PW_REG_DPM_PMIC_REQ_MASK_B] = "reg_dpm_pmic_req_mask_b",
	[PW_REG_ILDO_VCORE_REQ_MASK_B] = "reg_ildo_vcore_req_mask_b",

	/* SPM_SRC_MASK_5 */
	[PW_REG_DPM_SRCCLKENA_MASK_B] = "reg_dpm_srcclkena_mask_b",
	[PW_REG_DPM_VCORE_REQ_MASK_B] = "reg_dpm_vcore_req_mask_b",
	[PW_REG_DPM_VRF18_REQ_MASK_B] = "reg_dpm_vrf18_req_mask_b",
	[PW_REG_DPMAIF_APSRC_REQ_MASK_B] = "reg_dpmaif_apsrc_req_mask_b",
	[PW_REG_DPMAIF_CHIFR_DATA_COH_REQ_MASK_B] = "reg_dpmaif_chifr_data_coh_req_mask_b",
	[PW_REG_DPMAIF_CHIFR_TCU_COH_REQ_MASK_B] = "reg_dpmaif_chifr_tcu_coh_req_mask_b",
	[PW_REG_DPMAIF_EMI_REQ_MASK_B] = "reg_dpmaif_emi_req_mask_b",
	[PW_REG_DPMAIF_INFRA_REQ_MASK_B] = "reg_dpmaif_infra_req_mask_b",
	[PW_REG_DPMAIF_PMIC_REQ_MASK_B] = "reg_dpmaif_pmic_req_mask_b",
	[PW_REG_DPMAIF_SRCCLKENA_MASK_B] = "reg_dpmaif_srcclkena_mask_b",
	[PW_REG_DPMAIF_VCORE_REQ_MASK_B] = "reg_dpmaif_vcore_req_mask_b",
	[PW_REG_DPMAIF_VRF18_REQ_MASK_B] = "reg_dpmaif_vrf18_req_mask_b",
	[PW_REG_DVFSRC_LEVEL_REQ_MASK_B] = "reg_dvfsrc_level_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_APSRC_REQ_MASK_B] = "reg_emi_infra_rv33_apsrc_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_CHIFR_DATA_COH_REQ_MASK_B] = "reg_emi_infra_rv33_chifr_data_coh_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_CHIFR_TCU_COH_REQ_MASK_B] = "reg_emi_infra_rv33_chifr_tcu_coh_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_DCS_REQ_MASK_B] = "reg_emi_infra_rv33_dcs_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_INFRA_REQ_MASK_B] = "reg_emi_infra_rv33_infra_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_PMIC_REQ_MASK_B] = "reg_emi_infra_rv33_pmic_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_SRCCLKENA_MASK_B] = "reg_emi_infra_rv33_srcclkena_mask_b",
	[PW_REG_EMI_INFRA_RV33_VCORE_REQ_MASK_B] = "reg_emi_infra_rv33_vcore_req_mask_b",
	[PW_REG_EMI_INFRA_RV33_VRF18_REQ_MASK_B] = "reg_emi_infra_rv33_vrf18_req_mask_b",
	[PW_REG_EMISYS_APSRC_REQ_MASK_B] = "reg_emisys_apsrc_req_mask_b",

	/* SPM_SRC_MASK_6 */
	[PW_REG_GPUEB_APSRC_REQ_MASK_B] = "reg_gpueb_apsrc_req_mask_b",
	[PW_REG_GPUEB_CHIFR_DATA_COH_REQ_MASK_B] = "reg_gpueb_chifr_data_coh_req_mask_b",
	[PW_REG_GPUEB_CHIFR_TCU_COH_REQ_MASK_B] = "reg_gpueb_chifr_tcu_coh_req_mask_b",
	[PW_REG_GPUEB_EMI_REQ_MASK_B] = "reg_gpueb_emi_req_mask_b",
	[PW_REG_GPUEB_INFRA_REQ_MASK_B] = "reg_gpueb_infra_req_mask_b",
	[PW_REG_GPUEB_PMIC_REQ_MASK_B] = "reg_gpueb_pmic_req_mask_b",
	[PW_REG_GPUEB_SRCCLKENA_MASK_B] = "reg_gpueb_srcclkena_mask_b",
	[PW_REG_GPUEB_VCORE_REQ_MASK_B] = "reg_gpueb_vcore_req_mask_b",
	[PW_REG_GPUEB_VRF18_REQ_MASK_B] = "reg_gpueb_vrf18_req_mask_b",
	[PW_REG_HWCCF_APSRC_REQ_MASK_B] = "reg_hwccf_apsrc_req_mask_b",
	[PW_REG_HWCCF_EMI_REQ_MASK_B] = "reg_hwccf_emi_req_mask_b",
	[PW_REG_HWCCF_INFRA_REQ_MASK_B] = "reg_hwccf_infra_req_mask_b",
	[PW_REG_HWCCF_PMIC_REQ_MASK_B] = "reg_hwccf_pmic_req_mask_b",
	[PW_REG_HWCCF_SRCCLKENA_MASK_B] = "reg_hwccf_srcclkena_mask_b",
	[PW_REG_HWCCF_VCORE_REQ_MASK_B] = "reg_hwccf_vcore_req_mask_b",
	[PW_REG_HWCCF_VRF18_REQ_MASK_B] = "reg_hwccf_vrf18_req_mask_b",
	[PW_REG_IPIC_INFRA_REQ_MASK_B] = "reg_ipic_infra_req_mask_b",
	[PW_REG_IPIC_VRF18_REQ_MASK_B] = "reg_ipic_vrf18_req_mask_b",
	[PW_REG_MCU_APSRC_REQ_MASK_B] = "reg_mcu_apsrc_req_mask_b",
	[PW_REG_MCU_EMI_REQ_MASK_B] = "reg_mcu_emi_req_mask_b",
	[PW_REG_MCU_INFRA_REQ_MASK_B] = "reg_mcu_infra_req_mask_b",
	[PW_REG_MCU_PMIC_REQ_MASK_B] = "reg_mcu_pmic_req_mask_b",
	[PW_REG_MCU_SRCCLKENA_MASK_B] = "reg_mcu_srcclkena_mask_b",
	[PW_REG_MCU_VCORE_REQ_MASK_B] = "reg_mcu_vcore_req_mask_b",
	[PW_REG_MCU_VRF18_REQ_MASK_B] = "reg_mcu_vrf18_req_mask_b",
	[PW_REG_MD_APSRC_REQ_MASK_B] = "reg_md_apsrc_req_mask_b",
	[PW_REG_MD_DCS_REQ_MASK_B] = "reg_md_dcs_req_mask_b",
	[PW_REG_MD_EMI_REQ_MASK_B] = "reg_md_emi_req_mask_b",
	[PW_REG_MD_INFRA_REQ_MASK_B] = "reg_md_infra_req_mask_b",
	[PW_REG_MD_PMIC_REQ_MASK_B] = "reg_md_pmic_req_mask_b",
	[PW_REG_MD_SRCCLKENA_MASK_B] = "reg_md_srcclkena_mask_b",

	/* SPM_SRC_MASK_7 */
	[PW_REG_MD_SRCCLKENA1_MASK_B] = "reg_md_srcclkena1_mask_b",
	[PW_REG_MD_VCORE_REQ_MASK_B] = "reg_md_vcore_req_mask_b",
	[PW_REG_MD_VRF18_REQ_MASK_B] = "reg_md_vrf18_req_mask_b",
	[PW_REG_MM_PROC_APSRC_REQ_MASK_B] = "reg_mm_proc_apsrc_req_mask_b",
	[PW_REG_MM_PROC_CHIFR_DATA_COH_REQ_MASK_B] = "reg_mm_proc_chifr_data_coh_req_mask_b",
	[PW_REG_MM_PROC_CHIFR_TCU_COH_REQ_MASK_B] = "reg_mm_proc_chifr_tcu_coh_req_mask_b",
	[PW_REG_MM_PROC_EMI_REQ_MASK_B] = "reg_mm_proc_emi_req_mask_b",
	[PW_REG_MM_PROC_INFRA_REQ_MASK_B] = "reg_mm_proc_infra_req_mask_b",
	[PW_REG_MM_PROC_PMIC_REQ_MASK_B] = "reg_mm_proc_pmic_req_mask_b",
	[PW_REG_MM_PROC_SRCCLKENA_MASK_B] = "reg_mm_proc_srcclkena_mask_b",
	[PW_REG_MM_PROC_VCORE_REQ_MASK_B] = "reg_mm_proc_vcore_req_mask_b",
	[PW_REG_MM_PROC_VRF18_REQ_MASK_B] = "reg_mm_proc_vrf18_req_mask_b",
	[PW_REG_PCIE0_APSRC_REQ_MASK_B] = "reg_pcie0_apsrc_req_mask_b",
	[PW_REG_PCIE0_EMI_REQ_MASK_B] = "reg_pcie0_emi_req_mask_b",
	[PW_REG_PCIE0_INFRA_REQ_MASK_B] = "reg_pcie0_infra_req_mask_b",
	[PW_REG_PCIE0_PMIC_REQ_MASK_B] = "reg_pcie0_pmic_req_mask_b",
	[PW_REG_PCIE0_SRCCLKENA_MASK_B] = "reg_pcie0_srcclkena_mask_b",
	[PW_REG_PCIE0_VCORE_REQ_MASK_B] = "reg_pcie0_vcore_req_mask_b",
	[PW_REG_PCIE0_VRF18_REQ_MASK_B] = "reg_pcie0_vrf18_req_mask_b",
	[PW_REG_PCIE1_APSRC_REQ_MASK_B] = "reg_pcie1_apsrc_req_mask_b",
	[PW_REG_PCIE1_EMI_REQ_MASK_B] = "reg_pcie1_emi_req_mask_b",
	[PW_REG_PCIE1_INFRA_REQ_MASK_B] = "reg_pcie1_infra_req_mask_b",
	[PW_REG_PCIE1_PMIC_REQ_MASK_B] = "reg_pcie1_pmic_req_mask_b",
	[PW_REG_PCIE1_SRCCLKENA_MASK_B] = "reg_pcie1_srcclkena_mask_b",
	[PW_REG_PCIE1_VCORE_REQ_MASK_B] = "reg_pcie1_vcore_req_mask_b",
	[PW_REG_PCIE1_VRF18_REQ_MASK_B] = "reg_pcie1_vrf18_req_mask_b",
	[PW_REG_PERISYS_APSRC_REQ_MASK_B] = "reg_perisys_apsrc_req_mask_b",
	[PW_REG_PERISYS_EMI_REQ_MASK_B] = "reg_perisys_emi_req_mask_b",
	[PW_REG_PERISYS_INFRA_REQ_MASK_B] = "reg_perisys_infra_req_mask_b",
	[PW_REG_PERISYS_SRCCLKENA_MASK_B] = "reg_perisys_srcclkena_mask_b",
	[PW_REG_PERISYS_VRF18_REQ_MASK_B] = "reg_perisys_vrf18_req_mask_b",

	/* SPM_SRC_MASK_8 */
	[PW_REG_PLL_PMIC_REQ_MASK_B] = "reg_pll_pmic_req_mask_b",
	[PW_REG_PLL_SRCCLKENA_MASK_B] = "reg_pll_srcclkena_mask_b",

	/* SPM_SRC_MASK_9 */
	[PW_REG_PLL_VCORE_REQ_MASK_B] = "reg_pll_vcore_req_mask_b",
	[PW_REG_SCP_AOV_REQ_MASK_B] = "reg_scp_aov_req_mask_b",
	[PW_REG_SCP_APSRC_REQ_MASK_B] = "reg_scp_apsrc_req_mask_b",
	[PW_REG_SCP_EMI_REQ_MASK_B] = "reg_scp_emi_req_mask_b",
	[PW_REG_SCP_INFRA_REQ_MASK_B] = "reg_scp_infra_req_mask_b",
	[PW_REG_SCP_PMIC_REQ_MASK_B] = "reg_scp_pmic_req_mask_b",
	[PW_REG_SCP_SRCCLKENA_MASK_B] = "reg_scp_srcclkena_mask_b",
	[PW_REG_SCP_VCORE_REQ_MASK_B] = "reg_scp_vcore_req_mask_b",
	[PW_REG_SCP_VRF18_REQ_MASK_B] = "reg_scp_vrf18_req_mask_b",
	[PW_REG_HWCCF_AOV_REQ_MASK_B] = "reg_hwccf_aov_req_mask_b",

	/* SPM_SRC_MASK_10 */
	[PW_REG_SPM_SW_RSV_APSRC_REQ_MASK_B] = "reg_spm_sw_rsv_apsrc_req_mask_b",
	[PW_REG_SPM_SW_RSV_CHIFR_DATA_COH_REQ_MASK_B] = "reg_spm_sw_rsv_chifr_data_coh_req_mask_b",

	/* SPM_SRC_MASK_11 */
	[PW_REG_SPM_SW_RSV_CHIFR_TCU_COH_REQ_MASK_B] = "reg_spm_sw_rsv_chifr_tcu_coh_req_mask_b",
	[PW_REG_SPM_SW_RSV_EMI_REQ_MASK_B] = "reg_spm_sw_rsv_emi_req_mask_b",

	/* SPM_SRC_MASK_12 */
	[PW_REG_SPM_SW_RSV_INFRA_REQ_MASK_B] = "reg_spm_sw_rsv_infra_req_mask_b",
	[PW_REG_SPM_SW_RSV_PMIC_REQ_MASK_B] = "reg_spm_sw_rsv_pmic_req_mask_b",

	/* SPM_SRC_MASK_13 */
	[PW_REG_SPM_SW_RSV_SRCCLKENA_MASK_B] = "reg_spm_sw_rsv_srcclkena_mask_b",
	[PW_REG_SPM_SW_RSV_VCORE_REQ_MASK_B] = "reg_spm_sw_rsv_vcore_req_mask_b",

	/* SPM_SRC_MASK_14 */
	[PW_REG_SPM_SW_RSV_VRF18_REQ_MASK_B] = "reg_spm_sw_rsv_vrf18_req_mask_b",
	[PW_REG_SPU_HWROT_APSRC_REQ_MASK_B] = "reg_spu_hwrot_apsrc_req_mask_b",
	[PW_REG_SPU_HWROT_EMI_REQ_MASK_B] = "reg_spu_hwrot_emi_req_mask_b",
	[PW_REG_SPU_HWROT_INFRA_REQ_MASK_B] = "reg_spu_hwrot_infra_req_mask_b",
	[PW_REG_SPU_HWROT_PMIC_REQ_MASK_B] = "reg_spu_hwrot_pmic_req_mask_b",
	[PW_REG_SPU_HWROT_SRCCLKENA_MASK_B] = "reg_spu_hwrot_srcclkena_mask_b",
	[PW_REG_SPU_HWROT_VCORE_REQ_MASK_B] = "reg_spu_hwrot_vcore_req_mask_b",
	[PW_REG_SPU_HWROT_VRF18_REQ_MASK_B] = "reg_spu_hwrot_vrf18_req_mask_b",
	[PW_REG_SPU_ISE_APSRC_REQ_MASK_B] = "reg_spu_ise_apsrc_req_mask_b",
	[PW_REG_SPU_ISE_EMI_REQ_MASK_B] = "reg_spu_ise_emi_req_mask_b",
	[PW_REG_SPU_ISE_INFRA_REQ_MASK_B] = "reg_spu_ise_infra_req_mask_b",
	[PW_REG_SPU_ISE_PMIC_REQ_MASK_B] = "reg_spu_ise_pmic_req_mask_b",
	[PW_REG_SPU_ISE_SRCCLKENA_MASK_B] = "reg_spu_ise_srcclkena_mask_b",
	[PW_REG_SPU_ISE_VCORE_REQ_MASK_B] = "reg_spu_ise_vcore_req_mask_b",
	[PW_REG_SPU_ISE_VRF18_REQ_MASK_B] = "reg_spu_ise_vrf18_req_mask_b",
	[PW_REG_SRCCLKENI_INFRA_REQ_MASK_B] = "reg_srcclkeni_infra_req_mask_b",

	/* SPM_SRC_MASK_15 */
	[PW_REG_SRCCLKENI_PMIC_REQ_MASK_B] = "reg_srcclkeni_pmic_req_mask_b",
	[PW_REG_SRCCLKENI_SRCCLKENA_MASK_B] = "reg_srcclkeni_srcclkena_mask_b",
	[PW_REG_SRCCLKENI_VCORE_REQ_MASK_B] = "reg_srcclkeni_vcore_req_mask_b",
	[PW_REG_SSPM_APSRC_REQ_MASK_B] = "reg_sspm_apsrc_req_mask_b",
	[PW_REG_SSPM_CHIFR_DATA_COH_REQ_MASK_B] = "reg_sspm_chifr_data_coh_req_mask_b",
	[PW_REG_SSPM_CHIFR_TCU_COH_REQ_MASK_B] = "reg_sspm_chifr_tcu_coh_req_mask_b",
	[PW_REG_SSPM_EMI_REQ_MASK_B] = "reg_sspm_emi_req_mask_b",
	[PW_REG_SSPM_INFRA_REQ_MASK_B] = "reg_sspm_infra_req_mask_b",
	[PW_REG_SSPM_PMIC_REQ_MASK_B] = "reg_sspm_pmic_req_mask_b",
	[PW_REG_SSPM_SRCCLKENA_MASK_B] = "reg_sspm_srcclkena_mask_b",
	[PW_REG_SSPM_VRF18_REQ_MASK_B] = "reg_sspm_vrf18_req_mask_b",
	[PW_REG_SSRSYS_APSRC_REQ_MASK_B] = "reg_ssrsys_apsrc_req_mask_b",
	[PW_REG_SSRSYS_EMI_REQ_MASK_B] = "reg_ssrsys_emi_req_mask_b",
	[PW_REG_SSRSYS_INFRA_REQ_MASK_B] = "reg_ssrsys_infra_req_mask_b",
	[PW_REG_SSRSYS_PMIC_REQ_MASK_B] = "reg_ssrsys_pmic_req_mask_b",
	[PW_REG_SSRSYS_SRCCLKENA_MASK_B] = "reg_ssrsys_srcclkena_mask_b",
	[PW_REG_SSRSYS_VRF18_REQ_MASK_B] = "reg_ssrsys_vrf18_req_mask_b",
	[PW_REG_UART_HUB_INFRA_REQ_MASK_B] = "reg_uart_hub_infra_req_mask_b",
	[PW_REG_UART_HUB_PMIC_REQ_MASK_B] = "reg_uart_hub_pmic_req_mask_b",
	[PW_REG_UART_HUB_SRCCLKENA_MASK_B] = "reg_uart_hub_srcclkena_mask_b",
	[PW_REG_UART_HUB_VCORE_REQ_MASK_B] = "reg_uart_hub_vcore_req_mask_b",
	[PW_REG_UART_HUB_VRF18_REQ_MASK_B] = "reg_uart_hub_vrf18_req_mask_b",
	[PW_REG_UFS0_APSRC_REQ_MASK_B] = "reg_ufs0_apsrc_req_mask_b",
	[PW_REG_UFS0_EMI_REQ_MASK_B] = "reg_ufs0_emi_req_mask_b",
	[PW_REG_UFS0_INFRA_REQ_MASK_B] = "reg_ufs0_infra_req_mask_b",
	[PW_REG_UFS0_PMIC_REQ_MASK_B] = "reg_ufs0_pmic_req_mask_b",
	[PW_REG_UFS0_SRCCLKENA_MASK_B] = "reg_ufs0_srcclkena_mask_b",
	[PW_REG_UFS0_VCORE_REQ_MASK_B] = "reg_ufs0_vcore_req_mask_b",
	[PW_REG_UFS0_VRF18_REQ_MASK_B] = "reg_ufs0_vrf18_req_mask_b",

	/* SPM_SRC_MASK_16 */
	[PW_REG_UFS1_APSRC_REQ_MASK_B] = "reg_ufs1_apsrc_req_mask_b",
	[PW_REG_UFS1_EMI_REQ_MASK_B] = "reg_ufs1_emi_req_mask_b",
	[PW_REG_UFS1_INFRA_REQ_MASK_B] = "reg_ufs1_infra_req_mask_b",
	[PW_REG_UFS1_PMIC_REQ_MASK_B] = "reg_ufs1_pmic_req_mask_b",
	[PW_REG_UFS1_SRCCLKENA_MASK_B] = "reg_ufs1_srcclkena_mask_b",
	[PW_REG_UFS1_VCORE_REQ_MASK_B] = "reg_ufs1_vcore_req_mask_b",
	[PW_REG_UFS1_VRF18_REQ_MASK_B] = "reg_ufs1_vrf18_req_mask_b",
	[PW_REG_USB_APSRC_REQ_MASK_B] = "reg_usb_apsrc_req_mask_b",
	[PW_REG_USB_EMI_REQ_MASK_B] = "reg_usb_emi_req_mask_b",
	[PW_REG_USB_INFRA_REQ_MASK_B] = "reg_usb_infra_req_mask_b",
	[PW_REG_USB_PMIC_REQ_MASK_B] = "reg_usb_pmic_req_mask_b",
	[PW_REG_USB_SRCCLKENA_MASK_B] = "reg_usb_srcclkena_mask_b",
	[PW_REG_USB_VCORE_REQ_MASK_B] = "reg_usb_vcore_req_mask_b",
	[PW_REG_USB_VRF18_REQ_MASK_B] = "reg_usb_vrf18_req_mask_b",
	[PW_REG_VLP_PMSR_APSRC_REQ_MASK_B] = "reg_vlp_pmsr_apsrc_req_mask_b",
	[PW_REG_VLP_PMSR_EMI_REQ_MASK_B] = "reg_vlp_pmsr_emi_req_mask_b",
	[PW_REG_VLP_PMSR_INFRA_REQ_MASK_B] = "reg_vlp_pmsr_infra_req_mask_b",
	[PW_REG_VLP_PMSR_PMIC_REQ_MASK_B] = "reg_vlp_pmsr_pmic_req_mask_b",
	[PW_REG_VLP_PMSR_SRCCLKENA_MASK_B] = "reg_vlp_pmsr_srcclkena_mask_b",
	[PW_REG_VLP_PMSR_VCORE_REQ_MASK_B] = "reg_vlp_pmsr_vcore_req_mask_b",
	[PW_REG_VLP_PMSR_VRF18_REQ_MASK_B] = "reg_vlp_pmsr_vrf18_req_mask_b",
	[PW_REG_VLPCFG_RSV0_APSRC_REQ_MASK_B] = "reg_vlpcfg_rsv0_apsrc_req_mask_b",
	[PW_REG_VLPCFG_RSV0_EMI_REQ_MASK_B] = "reg_vlpcfg_rsv0_emi_req_mask_b",
	[PW_REG_VLPCFG_RSV0_INFRA_REQ_MASK_B] = "reg_vlpcfg_rsv0_infra_req_mask_b",
	[PW_REG_VLPCFG_RSV0_PMIC_REQ_MASK_B] = "reg_vlpcfg_rsv0_pmic_req_mask_b",
	[PW_REG_VLPCFG_RSV0_SRCCLKENA_MASK_B] = "reg_vlpcfg_rsv0_srcclkena_mask_b",
	[PW_REG_VLPCFG_RSV0_VCORE_REQ_MASK_B] = "reg_vlpcfg_rsv0_vcore_req_mask_b",
	[PW_REG_VLPCFG_RSV0_VRF18_REQ_MASK_B] = "reg_vlpcfg_rsv0_vrf18_req_mask_b",
	[PW_REG_VLPCFG_RSV1_APSRC_REQ_MASK_B] = "reg_vlpcfg_rsv1_apsrc_req_mask_b",
	[PW_REG_VLPCFG_RSV1_EMI_REQ_MASK_B] = "reg_vlpcfg_rsv1_emi_req_mask_b",
	[PW_REG_VLPCFG_RSV1_INFRA_REQ_MASK_B] = "reg_vlpcfg_rsv1_infra_req_mask_b",
	[PW_REG_VLPCFG_RSV1_PMIC_REQ_MASK_B] = "reg_vlpcfg_rsv1_pmic_req_mask_b",

	/* SPM_SRC_MASK_17 */
	[PW_REG_VLPCFG_RSV1_SRCCLKENA_MASK_B] = "reg_vlpcfg_rsv1_srcclkena_mask_b",
	[PW_REG_VLPCFG_RSV1_VCORE_REQ_MASK_B] = "reg_vlpcfg_rsv1_vcore_req_mask_b",
	[PW_REG_VLPCFG_RSV1_VRF18_REQ_MASK_B] = "reg_vlpcfg_rsv1_vrf18_req_mask_b",
	[PW_REG_ZRAM_APSRC_REQ_MASK_B] = "reg_zram_apsrc_req_mask_b",
	[PW_REG_ZRAM_CHIFR_DATA_COH_REQ_MASK_B] = "reg_zram_chifr_data_coh_req_mask_b",
	[PW_REG_ZRAM_CHIFR_TCU_COH_REQ_MASK_B] = "reg_zram_chifr_tcu_coh_req_mask_b",
	[PW_REG_ZRAM_EMI_REQ_MASK_B] = "reg_zram_emi_req_mask_b",
	[PW_REG_ZRAM_INFRA_REQ_MASK_B] = "reg_zram_infra_req_mask_b",
	[PW_REG_ZRAM_PMIC_REQ_MASK_B] = "reg_zram_pmic_req_mask_b",
	[PW_REG_ZRAM_SRCCLKENA_MASK_B] = "reg_zram_srcclkena_mask_b",
	[PW_REG_ZRAM_VCORE_REQ_MASK_B] = "reg_zram_vcore_req_mask_b",
	[PW_REG_ZRAM_VRF18_REQ_MASK_B] = "reg_zram_vrf18_req_mask_b",
	[PW_REG_PMRC_GIP_M_VCORE_REQ_MASK_B] = "reg_pmrc_gip_m_vcore_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_AOV_REQ_MASK_B] = "reg_idle_governor_aov_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_APSRC_REQ_MASK_B] = "reg_idle_governor_apsrc_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_CHIFR_DATA_COH_REQ_MASK_B] = "reg_idle_governor_chifr_data_coh_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_CHIFR_TCU_COH_REQ_MASK_B] = "reg_idle_governor_chifr_tcu_coh_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_DCS_REQ_MASK_B] = "reg_idle_governor_dcs_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_EMI_REQ_MASK_B] = "reg_idle_governor_emi_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_INFRA_REQ_MASK_B] = "reg_idle_governor_infra_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_PMIC_REQ_MASK_B] = "reg_idle_governor_pmic_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_SRCCLKENA_MASK_B] = "reg_idle_governor_srcclkena_mask_b",
	[PW_REG_IDLE_GOVERNOR_SYSCO_REQ_MASK_B] = "reg_idle_governor_sysco_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_VCORE_REQ_MASK_B] = "reg_idle_governor_vcore_req_mask_b",
	[PW_REG_IDLE_GOVERNOR_VRF18_REQ_MASK_B] = "reg_idle_governor_vrf18_req_mask_b",
	[PW_REG_PMRC_GIP_P_VCORE_REQ_MASK_B] = "reg_pmrc_gip_p_vcore_req_mask_b",
	[PW_REG_GPUEB_SYSCO_REQ_MASK_B] = "reg_gpueb_sysco_req_mask_b",

	/* SPM_EVENT_CON_MISC */
	[PW_REG_SRCCLKEN_FAST_RESP] = "reg_srcclken_fast_resp",
	[PW_REG_CSYSPWRUP_ACK_MASK] = "reg_csyspwrup_ack_mask",

	/* SPM_SRC_MASK_RSV_0 */
	[PW_REG_ADSP_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_adsp_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_ADSP_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_adsp_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_APIFR_HASH_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_apifr_hash_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_APIFR_HASH_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_apifr_hash_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_APIFR_IO_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_apifr_io_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_APIFR_IO_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_apifr_io_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_APIFR_IO_SRCCLKENA_RSV_MASK_B] = "reg_apifr_io_srcclkena_rsv_mask_b",
	[PW_REG_APIFR_MEM_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_apifr_mem_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_APIFR_MEM_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_apifr_mem_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_AUDIO_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_audio_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_AUDIO_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_audio_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_AUDIO_EMI_REQ_RSV_MASK_B] = "reg_audio_emi_req_rsv_mask_b",
	[PW_REG_CCIF_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ccif_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_ILDO_APSRC_REQ_RSV_MASK_B] = "reg_ildo_apsrc_req_rsv_mask_b",
	[PW_REG_ILDO_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ildo_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_ILDO_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ildo_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_ILDO_EMI_REQ_RSV_MASK_B] = "reg_ildo_emi_req_rsv_mask_b",
	[PW_REG_ILDO_INFRA_REQ_RSV_MASK_B] = "reg_ildo_infra_req_rsv_mask_b",
	[PW_REG_ILDO_PMIC_REQ_RSV_MASK_B] = "reg_ildo_pmic_req_rsv_mask_b",
	[PW_REG_ILDO_SRCCLKENA_RSV_MASK_B] = "reg_ildo_srcclkena_rsv_mask_b",
	[PW_REG_ILDO_VRF18_REQ_RSV_MASK_B] = "reg_ildo_vrf18_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_1 */
	[PW_REG_CCIF_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ccif_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_CCIF_EMI_REQ_RSV_MASK_B] = "reg_ccif_emi_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_2 */
	[PW_REG_CCIF_PMIC_REQ_RSV_MASK_B] = "reg_ccif_pmic_req_rsv_mask_b",
	[PW_REG_CCIF_VRF18_REQ_RSV_MASK_B] = "reg_ccif_vrf18_req_rsv_mask_b",
	[PW_REG_CG_CHECK_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_cg_check_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_CG_CHECK_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_cg_check_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_CKSYS_APSRC_REQ_RSV_MASK_B] = "reg_cksys_apsrc_req_rsv_mask_b",
	[PW_REG_CKSYS_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_cksys_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_CKSYS_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_cksys_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_CKSYS_EMI_REQ_RSV_MASK_B] = "reg_cksys_emi_req_rsv_mask_b",
	[PW_REG_CKSYS_INFRA_REQ_RSV_MASK_B] = "reg_cksys_infra_req_rsv_mask_b",
	[PW_REG_CKSYS_PMIC_REQ_RSV_MASK_B] = "reg_cksys_pmic_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_3 */
	[PW_REG_CKSYS_VCORE_REQ_RSV_MASK_B] = "reg_cksys_vcore_req_rsv_mask_b",
	[PW_REG_CKSYS_VRF18_REQ_RSV_MASK_B] = "reg_cksys_vrf18_req_rsv_mask_b",
	[PW_REG_CKSYS1_APSRC_REQ_RSV_MASK_B] = "reg_cksys1_apsrc_req_rsv_mask_b",
	[PW_REG_CKSYS1_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_cksys1_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_CKSYS1_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_cksys1_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_CKSYS1_EMI_REQ_RSV_MASK_B] = "reg_cksys1_emi_req_rsv_mask_b",
	[PW_REG_CKSYS1_INFRA_REQ_RSV_MASK_B] = "reg_cksys1_infra_req_rsv_mask_b",
	[PW_REG_CKSYS1_VRF18_REQ_RSV_MASK_B] = "reg_cksys1_vrf18_req_rsv_mask_b",
	[PW_REG_CKSYS2_APSRC_REQ_RSV_MASK_B] = "reg_cksys2_apsrc_req_rsv_mask_b",
	[PW_REG_CKSYS2_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_cksys2_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_CKSYS2_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_cksys2_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_CKSYS2_EMI_REQ_RSV_MASK_B] = "reg_cksys2_emi_req_rsv_mask_b",
	[PW_REG_CKSYS2_INFRA_REQ_RSV_MASK_B] = "reg_cksys2_infra_req_rsv_mask_b",
	[PW_REG_CKSYS2_VRF18_REQ_RSV_MASK_B] = "reg_cksys2_vrf18_req_rsv_mask_b",
	[PW_REG_CONN_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_conn_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_CONN_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_conn_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_MCUPM_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_mcupm_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_MCUPM_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_mcupm_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_DPM_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_dpm_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_DPM_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_dpm_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_DVFSRC_APSRC_REQ_RSV_MASK_B] = "reg_dvfsrc_apsrc_req_rsv_mask_b",
	[PW_REG_DVFSRC_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_dvfsrc_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_DVFSRC_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_dvfsrc_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_DVFSRC_EMI_REQ_RSV_MASK_B] = "reg_dvfsrc_emi_req_rsv_mask_b",
	[PW_REG_DVFSRC_INFRA_REQ_RSV_MASK_B] = "reg_dvfsrc_infra_req_rsv_mask_b",
	[PW_REG_DVFSRC_PMIC_REQ_RSV_MASK_B] = "reg_dvfsrc_pmic_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_4 */
	[PW_REG_DVFSRC_SRCCLKENA_RSV_MASK_B] = "reg_dvfsrc_srcclkena_rsv_mask_b",
	[PW_REG_DVFSRC_VCORE_REQ_RSV_MASK_B] = "reg_dvfsrc_vcore_req_rsv_mask_b",
	[PW_REG_DVFSRC_VRF18_REQ_RSV_MASK_B] = "reg_dvfsrc_vrf18_req_rsv_mask_b",
	[PW_REG_EMI_INFRA_RV33_EMI_REQ_RSV_MASK_B] = "reg_emi_infra_rv33_emi_req_rsv_mask_b",
	[PW_REG_EMISYS_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_emisys_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_EMISYS_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_emisys_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_EMISYS_EMI_REQ_RSV_MASK_B] = "reg_emisys_emi_req_rsv_mask_b",
	[PW_REG_EMISYS_INFRA_REQ_RSV_MASK_B] = "reg_emisys_infra_req_rsv_mask_b",
	[PW_REG_EMISYS_PMIC_REQ_RSV_MASK_B] = "reg_emisys_pmic_req_rsv_mask_b",
	[PW_REG_EMISYS_SRCCLKENA_RSV_MASK_B] = "reg_emisys_srcclkena_rsv_mask_b",
	[PW_REG_EMISYS_VCORE_REQ_RSV_MASK_B] = "reg_emisys_vcore_req_rsv_mask_b",
	[PW_REG_EMISYS_VRF18_REQ_RSV_MASK_B] = "reg_emisys_vrf18_req_rsv_mask_b",
	[PW_REG_HWCCF_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_hwccf_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_HWCCF_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_hwccf_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_IPIC_APSRC_REQ_RSV_MASK_B] = "reg_ipic_apsrc_req_rsv_mask_b",
	[PW_REG_IPIC_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ipic_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_IPIC_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ipic_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_IPIC_EMI_REQ_RSV_MASK_B] = "reg_ipic_emi_req_rsv_mask_b",
	[PW_REG_IPIC_PMIC_REQ_RSV_MASK_B] = "reg_ipic_pmic_req_rsv_mask_b",
	[PW_REG_IPIC_SRCCLKENA_RSV_MASK_B] = "reg_ipic_srcclkena_rsv_mask_b",
	[PW_REG_IPIC_VCORE_REQ_RSV_MASK_B] = "reg_ipic_vcore_req_rsv_mask_b",
	[PW_REG_MCU_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_mcu_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_MCU_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_mcu_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_MD_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_md_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_MD_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_md_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PCIE0_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_pcie0_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PCIE0_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_pcie0_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PCIE1_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_pcie1_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PCIE1_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_pcie1_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PERISYS_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_perisys_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PERISYS_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_perisys_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PERISYS_PMIC_REQ_RSV_MASK_B] = "reg_perisys_pmic_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_5 */
	[PW_REG_PERISYS_VCORE_REQ_RSV_MASK_B] = "reg_perisys_vcore_req_rsv_mask_b",
	[PW_REG_PLL_APSRC_REQ_RSV_MASK_B] = "reg_pll_apsrc_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_6 */
	[PW_REG_PLL_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_pll_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PLL_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_pll_chifr_tcu_coh_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_7 */
	[PW_REG_PLL_EMI_REQ_RSV_MASK_B] = "reg_pll_emi_req_rsv_mask_b",
	[PW_REG_PLL_INFRA_REQ_RSV_MASK_B] = "reg_pll_infra_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_8 */
	[PW_REG_PLL_VRF18_REQ_RSV_MASK_B] = "reg_pll_vrf18_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_APSRC_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_apsrc_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_EMI_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_emi_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_INFRA_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_infra_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_PMIC_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_pmic_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_SRCCLKENA_RSV_MASK_B] = "reg_pmrc_gip_m_srcclkena_rsv_mask_b",
	[PW_REG_PMRC_GIP_M_VRF18_REQ_RSV_MASK_B] = "reg_pmrc_gip_m_vrf18_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_APSRC_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_apsrc_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_EMI_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_emi_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_INFRA_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_infra_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_PMIC_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_pmic_req_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_SRCCLKENA_RSV_MASK_B] = "reg_pmrc_gip_p_srcclkena_rsv_mask_b",
	[PW_REG_PMRC_GIP_P_VRF18_REQ_RSV_MASK_B] = "reg_pmrc_gip_p_vrf18_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_9 */
	[PW_REG_SCP_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_scp_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_SCP_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_scp_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_SPU_HWROT_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_spu_hwrot_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_SPU_HWROT_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_spu_hwrot_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_SPU_ISE_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_spu_ise_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_SPU_ISE_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_spu_ise_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_SRCCLKENI_APSRC_REQ_RSV_MASK_B] = "reg_srcclkeni_apsrc_req_rsv_mask_b",
	[PW_REG_SRCCLKENI_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_srcclkeni_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_SRCCLKENI_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_srcclkeni_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_SRCCLKENI_EMI_REQ_RSV_MASK_B] = "reg_srcclkeni_emi_req_rsv_mask_b",
	[PW_REG_SRCCLKENI_VRF18_REQ_RSV_MASK_B] = "reg_srcclkeni_vrf18_req_rsv_mask_b",
	[PW_REG_SSPM_VCORE_REQ_RSV_MASK_B] = "reg_sspm_vcore_req_rsv_mask_b",
	[PW_REG_SSRSYS_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ssrsys_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_SSRSYS_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ssrsys_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_SSRSYS_VCORE_REQ_RSV_MASK_B] = "reg_ssrsys_vcore_req_rsv_mask_b",
	[PW_REG_UART_HUB_APSRC_REQ_RSV_MASK_B] = "reg_uart_hub_apsrc_req_rsv_mask_b",
	[PW_REG_UART_HUB_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_uart_hub_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_UART_HUB_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_uart_hub_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_UART_HUB_EMI_REQ_RSV_MASK_B] = "reg_uart_hub_emi_req_rsv_mask_b",
	[PW_REG_UFS0_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ufs0_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_UFS0_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ufs0_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_UFS1_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_ufs1_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_UFS1_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_ufs1_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_USB_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_usb_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_USB_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_usb_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_VLP_PMSR_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_vlp_pmsr_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_VLP_PMSR_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_vlp_pmsr_chifr_tcu_coh_req_rsv_mask_b",

	/* SPM_SRC_MASK_RSV_10 */
	[PW_REG_VLPCFG_RSV0_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_vlpcfg_rsv0_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_VLPCFG_RSV0_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_vlpcfg_rsv0_chifr_tcu_coh_req_rsv_mask_b",
	[PW_REG_VLPCFG_RSV1_CHIFR_DATA_COH_REQ_RSV_MASK_B] = "reg_vlpcfg_rsv1_chifr_data_coh_req_rsv_mask_b",
	[PW_REG_VLPCFG_RSV1_CHIFR_TCU_COH_REQ_RSV_MASK_B] = "reg_vlpcfg_rsv1_chifr_tcu_coh_req_rsv_mask_b",

	/* PW_REG_WAKEUP_EVENT_L_MASK */
	[PW_REG_WAKEUP_EVENT_L_MASK] = "reg_wakeup_event_l_mask",

	/* PW_REG_WAKEUP_EVENT_H_MASK */
	[PW_REG_WAKEUP_EVENT_H_MASK] = "reg_wakeup_event_h_mask",

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	[PW_REG_EXT_WAKEUP_EVENT_MASK] = "reg_ext_wakeup_event_mask",
};

struct subsys_req plat_subsys_req[] = {
	{"md", SPM_REQ_STA_6, (0x3F << 26), SPM_REQ_STA_7, 0x7, 0},
	{"conn", SPM_REQ_STA_3, (0x3F << 26), SPM_REQ_STA_4, 0x3, 0},
	{"scp", SPM_REQ_STA_9, (0x7F << 17), 0, 0, 0},
	{"adsp", SPM_REQ_STA_0, (0x7F << 1), 0, 0, 0},
	{"ufs", SPM_REQ_STA_15, (0x7F << 25), SPM_REQ_STA_16, 0x7F, 0},
	{"apu", SPM_REQ_STA_0, (0x7 << 29), SPM_REQ_STA_1, 0x3F, 0},
	{"mm_proc", SPM_REQ_STA_7, (0x1FF << 3), 0, 0, 0},
	{"uarthub", SPM_REQ_STA_15, (0x1F << 20), 0, 0, 0},
	{"pcie", SPM_REQ_STA_7, (0x3FFF << 12), 0, 0, 0},
	{"srclkeni", SPM_REQ_STA_14, (0x3 << 30), SPM_REQ_STA_15, 0x3F, 0},
	{"spm", SPM_SRC_REQ_0, 0x8F38, SPM_SRC_REQ_1, 0x3, 0},
};

struct logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};
#define	STATE_NUM	10
#define	STATE_NAME_SIZE	15
struct logger_fired_info {
	unsigned int fired;
	unsigned int state_index;
	char state_name[STATE_NUM][STATE_NAME_SIZE];
	int fired_index;
};

static struct lpm_spm_wake_status wakesrc;

static struct lpm_log_helper log_help = {
	.wakesrc = &wakesrc,
	.cur = 0,
	.prev = 0,
};

static unsigned int lpm_get_last_suspend_wakesrc(void)
{
	struct lpm_spm_wake_status *wakesrc = log_help.wakesrc;

	return wakesrc->r12_last_suspend;
}


static int lpm_get_common_status(void)
{
	struct lpm_log_helper *help = &log_help;
	unsigned int smc_id;

	if (!help->wakesrc || !lpm_spm_base)
		return -EINVAL;
	smc_id = MT_SPM_DBG_SMC_COMMON_PWR_STAT;
	/* FIXME: common debug reg (SPMFW) */
	help->wakesrc->debug_flag2 = plat_mmio_read(PCM_WDT_LATCH_SPARE_2); // debug_flag2
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1); // debug_flag1

	/* set common sodi cnt and clr */
	lpm_smc_spm_dbg(smc_id, MT_LPM_SMC_ACT_SET, 0, 0);
	help->wakesrc->apsrc_cnt = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_D1_2, SPM_SLP_COUNT);
	help->wakesrc->emi_cnt	 = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_D2, SPM_SLP_COUNT);
	help->wakesrc->vrf18_cnt = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_D3, SPM_SLP_COUNT);
	help->wakesrc->infra_cnt = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_D4, SPM_SLP_COUNT);
	help->wakesrc->pmic_cnt  = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_D6X, SPM_SLP_COUNT);
	help->wakesrc->vcore_cnt = lpm_smc_spm_dbg(smc_id,
					MT_LPM_SMC_ACT_GET, SPM_STAT_VCORE, SPM_SLP_COUNT);


	help->wakesrc->req_sta0 = plat_mmio_read(SPM_REQ_STA_0);
	help->wakesrc->req_sta1 = plat_mmio_read(SPM_REQ_STA_1);
	help->wakesrc->req_sta2 = plat_mmio_read(SPM_REQ_STA_2);
	help->wakesrc->req_sta3 = plat_mmio_read(SPM_REQ_STA_3);
	help->wakesrc->req_sta4 = plat_mmio_read(SPM_REQ_STA_4);
	help->wakesrc->req_sta5 = plat_mmio_read(SPM_REQ_STA_5);
	help->wakesrc->req_sta6 = plat_mmio_read(SPM_REQ_STA_6);
	help->wakesrc->req_sta7 = plat_mmio_read(SPM_REQ_STA_7);
	help->wakesrc->req_sta8 = plat_mmio_read(SPM_REQ_STA_8);
	help->wakesrc->req_sta9 = plat_mmio_read(SPM_REQ_STA_9);
	help->wakesrc->req_sta10 = plat_mmio_read(SPM_REQ_STA_10);
	help->wakesrc->req_sta11 = plat_mmio_read(SPM_REQ_STA_11);
	help->wakesrc->req_sta12 = plat_mmio_read(SPM_REQ_STA_12);
	help->wakesrc->req_sta13 = plat_mmio_read(SPM_REQ_STA_13);
	help->wakesrc->req_sta14 = plat_mmio_read(SPM_REQ_STA_14);
	help->wakesrc->req_sta15 = plat_mmio_read(SPM_REQ_STA_15);
	help->wakesrc->req_sta16 = plat_mmio_read(SPM_REQ_STA_16);
	help->wakesrc->req_sta17 = plat_mmio_read(SPM_REQ_STA_17);
	help->wakesrc->req_sta_rsv0 = plat_mmio_read(SPM_REQ_STA_RSV_0);
	help->wakesrc->req_sta_rsv1 = plat_mmio_read(SPM_REQ_STA_RSV_1);
	help->wakesrc->req_sta_rsv2 = plat_mmio_read(SPM_REQ_STA_RSV_2);
	help->wakesrc->req_sta_rsv3 = plat_mmio_read(SPM_REQ_STA_RSV_3);
	help->wakesrc->req_sta_rsv4 = plat_mmio_read(SPM_REQ_STA_RSV_4);
	help->wakesrc->req_sta_rsv5 = plat_mmio_read(SPM_REQ_STA_RSV_5);
	help->wakesrc->req_sta_rsv6 = plat_mmio_read(SPM_REQ_STA_RSV_6);
	help->wakesrc->req_sta_rsv7 = plat_mmio_read(SPM_REQ_STA_RSV_7);
	help->wakesrc->req_sta_rsv8 = plat_mmio_read(SPM_REQ_STA_RSV_8);
	help->wakesrc->req_sta_rsv9 = plat_mmio_read(SPM_REQ_STA_RSV_9);
	help->wakesrc->req_sta_rsv10 = plat_mmio_read(SPM_REQ_STA_RSV_10);

	return 0;
}

static int lpm_log_common_info(void)
{
	struct lpm_spm_wake_status *wakesrc = log_help.wakesrc;
#define LOG_BUF_SIZE	256
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;

	lpm_get_common_status();
	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size,
			"Common: debug_flag = 0x%x, 0x%x, ",
			wakesrc->debug_flag2, wakesrc->debug_flag1);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size,
			"cnt = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, ",
			wakesrc->apsrc_cnt, wakesrc->emi_cnt,
			wakesrc->vrf18_cnt, wakesrc->infra_cnt,
			wakesrc->pmic_cnt , wakesrc->vcore_cnt);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size,
			"req_sta =  0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | ",
			wakesrc->req_sta0, wakesrc->req_sta1,
			wakesrc->req_sta2, wakesrc->req_sta3,
			wakesrc->req_sta4, wakesrc->req_sta5,
			wakesrc->req_sta6, wakesrc->req_sta7);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size,
			"0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x, ",
			wakesrc->req_sta8, wakesrc->req_sta9,
			wakesrc->req_sta10, wakesrc->req_sta11,
			wakesrc->req_sta12, wakesrc->req_sta13,
			wakesrc->req_sta14, wakesrc->req_sta15,
			wakesrc->req_sta16, wakesrc->req_sta17);

	log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size,
			"req_sta_rsv = 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x\n",
			wakesrc->req_sta_rsv0, wakesrc->req_sta_rsv1,
			wakesrc->req_sta_rsv2, wakesrc->req_sta_rsv3,
			wakesrc->req_sta_rsv4, wakesrc->req_sta_rsv5,
			wakesrc->req_sta_rsv6, wakesrc->req_sta_rsv7,
			wakesrc->req_sta_rsv8, wakesrc->req_sta_rsv9,
			wakesrc->req_sta_rsv10);


	pr_info("[name:spm&][SPM] %s", log_buf);

	return 0;
}

static int lpm_get_wakeup_status(void)
{
	struct lpm_log_helper *help = &log_help;

	if (!help->wakesrc || !lpm_spm_base)
		return -EINVAL;

	help->wakesrc->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT_L);
	help->wakesrc->r12_h = plat_mmio_read(SPM_BK_WAKE_EVENT_H);
	help->wakesrc->raw_sta = plat_mmio_read(SPM_WAKEUP_L_STA);
	help->wakesrc->raw_h_sta = plat_mmio_read(SPM_WAKEUP_H_STA);
	help->wakesrc->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	help->wakesrc->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_L_STA);
	help->wakesrc->md32pcm_wakeup_h_sta = plat_mmio_read(MD32PCM_WAKEUP_H_STA);
	help->wakesrc->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	help->wakesrc->src_req_0 = plat_mmio_read(SPM_SRC_REQ_0);
	help->wakesrc->src_req_1 = plat_mmio_read(SPM_SRC_REQ_1);

	/* backup of SPM_WAKEUP_MISC */
	help->wakesrc->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	help->wakesrc->timer_out = plat_mmio_read(SPM_BK_PCM_TIMER);

	/* get other SYS and co-clock status */
	help->wakesrc->r13 = plat_mmio_read(MD32PCM_SCU_STA0);
	help->wakesrc->req_sta0 = plat_mmio_read(SPM_REQ_STA_0);
	help->wakesrc->req_sta1 = plat_mmio_read(SPM_REQ_STA_1);
	help->wakesrc->req_sta2 = plat_mmio_read(SPM_REQ_STA_2);
	help->wakesrc->req_sta3 = plat_mmio_read(SPM_REQ_STA_3);
	help->wakesrc->req_sta4 = plat_mmio_read(SPM_REQ_STA_4);
	help->wakesrc->req_sta5 = plat_mmio_read(SPM_REQ_STA_5);
	help->wakesrc->req_sta6 = plat_mmio_read(SPM_REQ_STA_6);
	help->wakesrc->req_sta7 = plat_mmio_read(SPM_REQ_STA_7);
	help->wakesrc->req_sta8 = plat_mmio_read(SPM_REQ_STA_8);
	help->wakesrc->req_sta9 = plat_mmio_read(SPM_REQ_STA_9);
	help->wakesrc->req_sta10 = plat_mmio_read(SPM_REQ_STA_10);
	help->wakesrc->req_sta11 = plat_mmio_read(SPM_REQ_STA_11);
	help->wakesrc->req_sta12 = plat_mmio_read(SPM_REQ_STA_12);
	help->wakesrc->req_sta13 = plat_mmio_read(SPM_REQ_STA_13);
	help->wakesrc->req_sta14 = plat_mmio_read(SPM_REQ_STA_14);
	help->wakesrc->req_sta15 = plat_mmio_read(SPM_REQ_STA_15);
	help->wakesrc->req_sta16 = plat_mmio_read(SPM_REQ_STA_16);
	help->wakesrc->req_sta17 = plat_mmio_read(SPM_REQ_STA_17);

	help->wakesrc->req_sta_rsv0 = plat_mmio_read(SPM_REQ_STA_RSV_0);
	help->wakesrc->req_sta_rsv1 = plat_mmio_read(SPM_REQ_STA_RSV_1);
	help->wakesrc->req_sta_rsv2 = plat_mmio_read(SPM_REQ_STA_RSV_2);
	help->wakesrc->req_sta_rsv3 = plat_mmio_read(SPM_REQ_STA_RSV_3);
	help->wakesrc->req_sta_rsv4 = plat_mmio_read(SPM_REQ_STA_RSV_4);
	help->wakesrc->req_sta_rsv5 = plat_mmio_read(SPM_REQ_STA_RSV_5);
	help->wakesrc->req_sta_rsv6 = plat_mmio_read(SPM_REQ_STA_RSV_6);
	help->wakesrc->req_sta_rsv7 = plat_mmio_read(SPM_REQ_STA_RSV_7);
	help->wakesrc->req_sta_rsv8 = plat_mmio_read(SPM_REQ_STA_RSV_8);
	help->wakesrc->req_sta_rsv9 = plat_mmio_read(SPM_REQ_STA_RSV_9);
	help->wakesrc->req_sta_rsv10 = plat_mmio_read(SPM_REQ_STA_RSV_10);

	help->wakesrc->ack_sta_ulposc = plat_mmio_read(SPM_ULPOSC_ACK_STA);

	/* get debug flag for PCM execution check */
	help->wakesrc->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	help->wakesrc->b_sw_flag0 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->b_sw_flag1 = plat_mmio_read(SPM_SW_RSV_8);

	help->wakesrc->sw_rsv_0 = plat_mmio_read(SPM_SW_RSV_0);
	help->wakesrc->sw_rsv_1 = plat_mmio_read(SPM_SW_RSV_1);
	help->wakesrc->sw_rsv_2 = plat_mmio_read(SPM_SW_RSV_2);
	help->wakesrc->sw_rsv_3 = plat_mmio_read(SPM_SW_RSV_3);
	help->wakesrc->sw_rsv_4 = plat_mmio_read(SPM_SW_RSV_4);
	help->wakesrc->sw_rsv_5 = plat_mmio_read(SPM_SW_RSV_5);
	help->wakesrc->sw_rsv_6 = plat_mmio_read(SPM_SW_RSV_6);
	help->wakesrc->sw_rsv_7 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->sw_rsv_8 = plat_mmio_read(SPM_SW_RSV_8);

	help->wakesrc->debug_spare3 = plat_mmio_read(PCM_WDT_LATCH_SPARE_3);
	help->wakesrc->debug_spare4 = plat_mmio_read(PCM_WDT_LATCH_SPARE_4);
	help->wakesrc->debug_spare5 = plat_mmio_read(PCM_WDT_LATCH_SPARE_5);
	help->wakesrc->debug_spare6 = plat_mmio_read(PCM_WDT_LATCH_SPARE_6);
	help->wakesrc->debug_spare7 = plat_mmio_read(PCM_WDT_LATCH_SPARE_7);
	help->wakesrc->debug_spare8 = plat_mmio_read(PCM_WDT_LATCH_SPARE_8);
	help->wakesrc->debug_spare9 = plat_mmio_read(PCM_WDT_LATCH_SPARE_9);
	help->wakesrc->debug_spare10 = plat_mmio_read(PCM_WDT_LATCH_SPARE_10);
	help->wakesrc->debug_spare11 = plat_mmio_read(PCM_WDT_LATCH_SPARE_11);
	help->wakesrc->debug_spare12 = plat_mmio_read(PCM_WDT_LATCH_SPARE_12);
	help->wakesrc->debug_spare13 = plat_mmio_read(PCM_WDT_LATCH_SPARE_13);
	help->wakesrc->debug_spare14 = plat_mmio_read(PCM_WDT_LATCH_SPARE_14);
	help->wakesrc->debug_spare15 = plat_mmio_read(PCM_WDT_LATCH_SPARE_15);

	/* get ISR status */
	help->wakesrc->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get SW flag status */
	help->wakesrc->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	help->wakesrc->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	help->wakesrc->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);

	return 0;
}

static void lpm_save_sleep_info(void)
{
}

static void suspend_show_detailed_wakeup_reason
	(struct lpm_spm_wake_status *wakesta)
{
	unsigned long isr_num;
	if (wakesta->r12 == 0 && wakesta->r12_h == 0) {
		isr_num = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_SPM_EL1, MT_LPM_SMC_ACT_GET, 0, 0);
		pr_info("[name:spm&][SPM] Wake up by none, pending isr number = 0x%lx \n", isr_num);
	}
}

static int lpm_show_message(int type, const char *prefix, void *data)
{
	struct lpm_spm_wake_status *wakesrc = log_help.wakesrc;

#undef LOG_BUF_SIZE
	#define LOG_BUF_SIZE		256
	/* FIXME: check log buffer size */
	#define LOG_BUF_OUT_SZ		1024
	#define IS_WAKE_MISC(x)	(wakesrc->wake_misc & x)
	#define IS_LOGBUF(ptr, newstr) \
		((strlen(ptr) + strlen(newstr)) < LOG_BUF_SIZE)

	unsigned int spm_26M_off_pct = 0;
	unsigned int spm_vcore_off_pct = 0;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr = NULL;
	int i = 0, log_size = 0, log_type = 0;
	unsigned int wr = WR_UNKNOWN, ret = 0;;
	const char *scenario = prefix ?: "UNKNOWN";

	log_type = ((struct lpm_issuer *)data)->log_type;

	if (log_type == LOG_MCUSYS_NOT_OFF) {
		pr_info("[name:spm&][SPM] %s didn't enter mcusys off, mcusys off cnt is no update\n",
					scenario);
		wr =  WR_ABORT;

		goto end;
	}

	if (wakesrc->r12 & R12_TWAM_PMSR_DVFSRC) {
		if (IS_WAKE_MISC(WAKE_MISC_SRCLKEN_RC_ERR_INT)) {
			local_ptr = " SRCLKEN_RC_ERR_INT";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_TIMEOUT_WAKEUP_0)) {
			local_ptr = " SPM_TIMEOUT_WAKEUP_0";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_TIMEOUT_WAKEUP_1)) {
			local_ptr = " SPM_TIMEOUT_WAKEUP_1";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_TIMEOUT_WAKEUP_2)) {
			local_ptr = " SPM_TIMEOUT_WAKEUP_2";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_DVFSRC_IRQ)) {
			local_ptr = " DVFSRC";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_DVFSRC;
		}
		if (IS_WAKE_MISC(WAKE_MISC_TWAM_IRQ_B)) {
			local_ptr = " TWAM";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_TWAM;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_0)) {
			local_ptr = " SPM_ACK_CHK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_1)) {
			local_ptr = " SPM_ACK_CHK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_2)) {
			local_ptr = " SPM_ACK_CHK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_3)) {
			local_ptr = " SPM_ACK_CHK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL)) {
			local_ptr = " SPM_ACK_CHK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_VLP_BUS_TIMEOUT_IRQ)) {
			local_ptr = " VLP_BUS_TIMEOUT_IRQ";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_PMIC_EINT_OUT)) {
			local_ptr = " PMIC_EINT_OUT";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_PMIC_IRQ_ACK)) {
			local_ptr = " PMIC_IRQ_ACK";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
		if (IS_WAKE_MISC(WAKE_MISC_PMIC_SCP_IRQ)) {
			local_ptr = " PMIC_SCP_IRQ";
			if (IS_LOGBUF(buf, local_ptr))
				strncat(buf, local_ptr,
					strlen(local_ptr));
			wr = WR_SPM_ACK_CHK;
		}
	}
	for (i = 0; i < 32; i++) {
		if (wakesrc->r12 & (1U << i)) {
			if (IS_LOGBUF(buf, wakesrc_str[i]))
				strncat(buf, wakesrc_str[i],
					strlen(wakesrc_str[i]));

			wr = WR_WAKE_SRC;
		}
		if (wakesrc->r12_h & (1U << i)) {
			if (IS_LOGBUF(buf, wakesrc_h_str[i]))
				strncat(buf, wakesrc_h_str[i],
					strlen(wakesrc_h_str[i]));

			wr = WR_WAKE_SRC;
		}
	}
	WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
		scenario, buf, wakesrc->timer_out, wakesrc->r13,
		wakesrc->debug_flag, wakesrc->debug_flag1);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"r12 = 0x%x 0x%x, raw_sta = 0x%x 0x%x 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
		wakesrc->r12, wakesrc->r12_h,
		wakesrc->raw_sta,
		wakesrc->raw_h_sta,
		wakesrc->md32pcm_wakeup_sta,
		wakesrc->md32pcm_wakeup_h_sta,
		wakesrc->md32pcm_event_sta,
		wakesrc->idle_sta);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"req_sta = 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x",
		wakesrc->req_sta0, wakesrc->req_sta1, wakesrc->req_sta2,
		wakesrc->req_sta3, wakesrc->req_sta4, wakesrc->req_sta5,
		wakesrc->req_sta6, wakesrc->req_sta7, wakesrc->req_sta8,
		wakesrc->req_sta9, wakesrc->req_sta10, wakesrc->req_sta11);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		" | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x, ",
		wakesrc->req_sta12, wakesrc->req_sta13, wakesrc->req_sta14,
		wakesrc->req_sta15, wakesrc->req_sta16, wakesrc->req_sta17);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"req_sta_rsv = 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x, ",
		wakesrc->req_sta_rsv0, wakesrc->req_sta_rsv1, wakesrc->req_sta_rsv2,
		wakesrc->req_sta_rsv3, wakesrc->req_sta_rsv4, wakesrc->req_sta_rsv5,
		wakesrc->req_sta_rsv6, wakesrc->req_sta_rsv7, wakesrc->req_sta_rsv8,
		wakesrc->req_sta_rsv9, wakesrc->req_sta_rsv10);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"ack_sta_ulposc = 0x%x, ", wakesrc->ack_sta_ulposc);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"debug_spare3 = 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x, ",
		wakesrc->debug_spare3, wakesrc->debug_spare4, wakesrc->debug_spare5,
		wakesrc->debug_spare6, wakesrc->debug_spare7, wakesrc->debug_spare8,
		wakesrc->debug_spare9, wakesrc->debug_spare10, wakesrc->debug_spare11,
		wakesrc->debug_spare12, wakesrc->debug_spare13, wakesrc->debug_spare14,
		wakesrc->debug_spare15);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"spm_src_req = 0x%x 0x%x, ",
		wakesrc->src_req_0, wakesrc->src_req_1);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"isr = 0x%x, sw_rsv = 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x, ",
		wakesrc->isr,
		wakesrc->sw_rsv_0, wakesrc->sw_rsv_1, wakesrc->sw_rsv_2,
		wakesrc->sw_rsv_3, wakesrc->sw_rsv_4, wakesrc->sw_rsv_5,
		wakesrc->sw_rsv_6, wakesrc->sw_rsv_7, wakesrc->sw_rsv_8);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"raw_ext_sta = 0x%x, wake_misc = 0x%x, sw_flag = 0x%x 0x%x, b_sw_flag = 0x%x 0x%x, ",
		wakesrc->raw_ext_sta,
		wakesrc->wake_misc,
		wakesrc->sw_flag0, wakesrc->sw_flag1,
		wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);

	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_OUT_SZ - log_size,
		"clk_settle = 0x%x, ", wakesrc->clk_settle);

	if (type == LPM_ISSUER_SUSPEND && lpm_spm_base) {
		/* calculate 26M off percentage in suspend period */
		if (wakesrc->timer_out != 0) {
			spm_26M_off_pct =
				(100UL * plat_mmio_read(SPM_BK_VTCXO_DUR))
						/ wakesrc->timer_out;
			spm_vcore_off_pct =
				(100UL * plat_mmio_read(SPM_SW_RSV_4))
						/ wakesrc->timer_out;
		}
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d, vcore_off_pct = %d\n",
			plat_mmio_read(SYS_TIMER_VALUE_L),
			plat_mmio_read(SYS_TIMER_VALUE_H),
			spm_26M_off_pct, spm_vcore_off_pct);
	}

	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	if (type == LPM_ISSUER_SUSPEND) {
		pr_info("[name:spm&][SPM] %s", log_buf);
		suspend_show_detailed_wakeup_reason(wakesrc);
		lpm_dbg_spm_rsc_req_check(wakesrc->debug_flag);
		pr_info("[name:spm&][SPM] suspended for %d.%03d seconds",
			PCM_TICK_TO_SEC(wakesrc->timer_out),
			PCM_TICK_TO_SEC((wakesrc->timer_out %
				PCM_32K_TICKS_PER_SEC)
			* 1000));
		ret = lpm_smc_cpu_pm_lp(SUSPEND_ABORT_REASON, MT_LPM_SMC_ACT_GET, 0, 0);
		if (ret)
			pr_info("[name:spm&][SPM] platform abort reason = %u\n", ret);
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
		log_md_sleep_info();
#endif
		wakesrc->r12_last_suspend = wakesrc->r12;
		wakesrc->r12_h_last_suspend = wakesrc->r12_h;
	} else
		pr_info("[name:spm&][SPM] %s", log_buf);

	if (wakesrc->sw_flag1
		& (SPM_FLAG1_ENABLE_WAKE_PROF | SPM_FLAG1_ENABLE_SLEEP_PROF)) {
		uint32_t addr;

		for (addr = SYS_TIMER_LATCH_L_00; addr <= SYS_TIMER_LATCH_L_15; addr += 8) {
			pr_info("[name:spm&][SPM][timestamp] 0x%08x 0x%08x\n",
				plat_mmio_read(addr + 4), plat_mmio_read(addr));
		}
	}

end:
	return wr;
}

static struct lpm_dbg_plat_ops dbg_ops = {
	.lpm_show_message = lpm_show_message,
	.lpm_save_sleep_info = lpm_save_sleep_info,
	.lpm_get_spm_wakesrc_irq = NULL,
	.lpm_get_wakeup_status = lpm_get_wakeup_status,
	.lpm_log_common_status = lpm_log_common_info,
};

static struct lpm_dbg_plat_info dbg_info = {
	.spm_req = plat_subsys_req,
	.spm_req_num = sizeof(plat_subsys_req)/sizeof(struct subsys_req),
	.spm_req_sta_addr = SPM_REQ_STA_0,
	.spm_req_sta_num = SPM_REQ_STA_NUM,
};

static struct lpm_logger_mbrain_dbg_ops lpm_logger_mbrain_ops = {
	.get_last_suspend_wakesrc = lpm_get_last_suspend_wakesrc,
};

static int __init mt6993_dbg_device_initcall(void)
{
	int ret;

	lpm_dbg_plat_info_set(dbg_info);

	ret = lpm_dbg_plat_ops_register(&dbg_ops);
	if (ret)
		pr_info("[name:spm&][SPM] Failed to register dbg plat ops notifier.\n");

	lpm_spm_fs_init(pwr_ctrl_str, PW_MAX_COUNT);
	lpm_ocla_fs_init();

#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	ret = lpm_sys_res_stat_init();
	if(ret)
		pr_info("[name:spm&][SPM] Failed to init sys_res plat\n");
#endif

	ret = register_lpm_logger_mbrain_dbg_ops(&lpm_logger_mbrain_ops);
	if(ret)
		pr_info("[name:spm&][SPM] Failed to register lpm logger mbrain dbg ops\n");

	return 0;
}

static int __init mt6993_dbg_late_initcall(void)
{
	lpm_trace_event_init(plat_subsys_req, sizeof(plat_subsys_req)/sizeof(struct subsys_req));

	return 0;
}

int __init mt6993_dbg_init(void)
{
	int ret = 0;

	ret = mt6993_dbg_device_initcall();
	if (ret)
		goto mt6993_dbg_init_fail;

	ret = mt6993_dbg_late_initcall();
	if (ret)
		goto mt6993_dbg_init_fail;

	return 0;

mt6993_dbg_init_fail:
	return -EAGAIN;
}

void __exit mt6993_dbg_exit(void)
{
	lpm_trace_event_deinit();
#if IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)
	lpm_sys_res_stat_deinit();
#endif
	unregister_lpm_logger_mbrain_dbg_ops();
}

module_init(mt6993_dbg_init);
module_exit(mt6993_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mt6993 low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
