/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_INTERNAL_H__
#define __MTK_DPC_INTERNAL_H__

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#define MMSYS_MT6989  0x6989
#define MMSYS_MT6878  0x6878
#define MMSYS_MT6991  0x6991
#define MMSYS_MT6993  0x6993
#define MMSYS_MT6858  0x6858

#define DPCFUNC(fmt, args...) \
	pr_info("[dpc] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define DPCERR(fmt, args...) \
	pr_info("[dpc][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define DPCDUMP(fmt, args...) \
	pr_info("[dpc] " fmt "\n", ##args)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define DPCAEE(fmt, args...) \
	do {                                                                                     \
		char str[200];                                                                   \
		int r;                                                                           \
		pr_info("[dpc][err] %s:%d " fmt "\n", __func__, __LINE__, ##args);               \
		r = snprintf(str, 199, "DPC:" fmt, ##args);                                      \
		if (r < 0)                                                                       \
			pr_info("snprintf error\n");                                             \
		aee_kernel_warning_api(__FILE__, __LINE__,                                       \
				       DB_OPT_DEFAULT | DB_OPT_FTRACE | DB_OPT_MMPROFILE_BUFFER, \
				       str, fmt, ##args);                                        \
	} while (0)
#else /* !CONFIG_MTK_AEE_FEATURE */
#define DPCAEE(fmt, args...) \
	pr_info("[dpc][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)
#endif

/* Compatible with 32bit division and mold operation */
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
#define DO_COMMON_DIV(x, base) ((x) / (base))
#define DO_COMMMON_MOD(x, base) ((x) % (base))
#else
#define DO_COMMON_DIV(x, base) ({                   \
	uint64_t result = 0;                        \
	if (sizeof(x) < sizeof(uint64_t))           \
		result = ((x) / (base));            \
	else {                                      \
		uint64_t __x = (x);                 \
		do_div(__x, (base));                \
		result = __x;                       \
	}                                           \
	result;                                     \
})
#define DO_COMMMON_MOD(x, base) ({                  \
	uint32_t result = 0;                        \
	if (sizeof(x) < sizeof(uint64_t))           \
		result = ((x) % (base));            \
	else {                                      \
		uint64_t __x = (x);                 \
		result = do_div(__x, (base));       \
	}                                           \
	result;                                     \
})
#endif

enum GCE_COND_REVERSE_COND {
	R_CMDQ_NOT_EQUAL = CMDQ_EQUAL,
	R_CMDQ_EQUAL = CMDQ_NOT_EQUAL,
	R_CMDQ_LESS = CMDQ_GREATER_THAN_AND_EQUAL,
	R_CMDQ_GREATER = CMDQ_LESS_THAN_AND_EQUAL,
	R_CMDQ_LESS_EQUAL = CMDQ_GREATER_THAN,
	R_CMDQ_GREATER_EQUAL = CMDQ_LESS_THAN,
};

#define GCE_COND_DECLARE \
	u32 _inst_condi_jump, _inst_jump_end; \
	u64 _jump_pa; \
	u64 *_inst; \
	struct cmdq_pkt *_cond_pkt; \
	u16 _gpr, _reg_jump

#define GCE_COND_ASSIGN(pkt, addr, gpr) do { \
	_cond_pkt = pkt; \
	_reg_jump = addr; \
	_gpr = gpr; \
} while (0)

#define GCE_IF(lop, cond, rop) do { \
	_inst_condi_jump = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_assign_command(_cond_pkt, _reg_jump, 0); \
	cmdq_pkt_cond_jump_abs(_cond_pkt, _reg_jump, &lop, &rop, (enum CMDQ_CONDITION_ENUM) cond); \
	_inst_jump_end = _inst_condi_jump; \
} while (0)

#define GCE_ELSE do { \
	_inst_jump_end = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_jump_addr(_cond_pkt, 0); \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_condi_jump); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_FI do { \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_jump_end); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst & ((u64)0xFFFFFFFF << 32); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_FI_REUSE(reuse) do { \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_jump_end); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst & ((u64)0xFFFFFFFF << 32); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
	(reuse)->op = CMDQ_CODE_JUMP_C_ABSOLUTE; \
	(reuse)->offset = _inst_condi_jump; \
	(reuse)->val = _cond_pkt->cmd_buf_size; \
} while (0)

#define VLP_DISP_SW_VOTE_CON 0x410
#define VLP_DISP_SW_VOTE_SET 0x414
#define VLP_DISP_SW_VOTE_CLR 0x418
#define SPM_DIS0_PWR_CON 0xE98
#define SPM_DIS1_PWR_CON 0xE9C
#define SPM_OVL0_PWR_CON 0xEA0
#define SPM_OVL1_PWR_CON 0xEA4
#define SPM_MML1_PWR_CON 0xE94
#define SPM_PWR_STATUS_MSB 0xf78 /* vcore[3] mml[4][5] dis[6][7] ovl[8][9] mminfra[10] */

#define DISP_REG_DPC_EN                                  0x000UL
#define DISP_REG_DPC_RESET                               0x004UL
#define DISP_REG_DPC_MERGE_DISP_INT_CFG                  0x008UL
#define DISP_REG_DPC_MERGE_MML_INT_CFG                   0x00CUL
#define DISP_REG_DPC_MERGE_DISP_INTSTA                   0x010UL
#define DISP_REG_DPC_MERGE_MML_INTSTA                    0x014UL
#define DISP_REG_DPC_MERGE_DISP_UP_INTSTA                0x018UL
#define DISP_REG_DPC_MERGE_MML_UP_INTSTA                 0x01CUL
#define DISP_REG_DPC_DISP_INTEN                          0x030UL
#define DISP_REG_DPC_DISP_INTSTA                         0x034UL
#define DISP_REG_DPC_DISP_UP_INTEN                       0x038UL
#define DISP_REG_DPC_DISP_UP_INTSTA                      0x03CUL
#define DISP_REG_DPC_MML_INTEN                           0x040UL
#define DISP_REG_DPC_MML_INTSTA                          0x044UL
#define DISP_REG_DPC_MML_UP_INTEN                        0x048UL
#define DISP_REG_DPC_MML_UP_INTSTA                       0x04CUL
#define DISP_REG_DPC_DISP_POWER_STATE_CFG                0x050UL
#define DISP_REG_DPC_MML_POWER_STATE_CFG                 0x054UL
#define DISP_REG_DPC_DISP_MASK_CFG                       0x060UL
#define DISP_REG_DPC_MML_MASK_CFG                        0x064UL
#define DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG              0x068UL
#define DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG               0x06CUL
#define DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG                0x070UL
#define DISP_REG_DPC_MML_HRTBW_SRTBW_CFG                 0x074UL
#define DISP_REG_DPC_DISP_HIGH_HRT_BW                    0x078UL
#define DISP_REG_DPC_DISP_LOW_HRT_BW                     0x07CUL
#define DISP_REG_DPC_DISP_SW_SRT_BW                      0x080UL
#define DISP_REG_DPC_MML_SW_HRT_BW                       0x084UL
#define DISP_REG_DPC_MML_SW_SRT_BW                       0x088UL
#define DISP_REG_DPC_DISP_VDISP_DVFS_CFG                 0x090UL
#define DISP_REG_DPC_MML_VDISP_DVFS_CFG                  0x094UL
#define DISP_REG_DPC_DISP_VDISP_DVFS_VAL                 0x098UL
#define DISP_REG_DPC_MML_VDISP_DVFS_VAL                  0x09CUL
#define DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG              0x0A0UL
#define DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG               0x0A4UL
#define DISP_REG_DPC_EVENT_TYPE                          0x0B0UL
#define DISP_REG_DPC_EVENT_EN                            0x0B4UL
#define DISP_REG_DPC_HW_DCM                              0x0B8UL
#define DISP_REG_DPC_ACT_SWITCH_CFG                      0x0BCUL
#define DISP_REG_DPC_DDREN_ACK_SEL                       0x0C0UL
#define DISP_REG_DPC_DISP_EXT_INPUT_EN                   0x0C4UL
#define DISP_REG_DPC_MML_EXT_INPUT_EN                    0x0C8UL
#define DISP_REG_DPC_DISP_DT_CFG                         0x0D0UL
#define DISP_REG_DPC_MML_DT_CFG                          0x0D4UL
#define DISP_REG_DPC_DISP_DT_FOLLOW_CFG                  0x0E8UL
#define DISP_REG_DPC_MML_DT_FOLLOW_CFG                   0x0ECUL
#define DISP_REG_DPC_DTx_COUNTER(n)                      (0x100UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DTx_SW_TRIG(n)                      (0x200UL + 0x4 * (n))	// n = 0 ~ 56
#define DISP_REG_DPC_DISP0_MTCMOS_CFG                    0x300UL
#define DISP_REG_DPC_DISP0_MTCMOS_ON_DELAY_CFG           0x304UL
#define DISP_REG_DPC_DISP0_MTCMOS_STA                    0x308UL
#define DISP_REG_DPC_DISP0_MTCMOS_STATE_STA              0x30CUL
#define DISP_REG_DPC_DISP0_MTCMOS_OFF_PROT_CFG           0x310UL
#define DISP_REG_DPC_DISP0_THREADx_SET(n)                (0x320UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CLR(n)                (0x340UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP0_THREADx_CFG(n)                (0x360UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_MTCMOS_CFG                    0x400UL
#define DISP_REG_DPC_DISP1_MTCMOS_ON_DELAY_CFG           0x404UL
#define DISP_REG_DPC_DISP1_MTCMOS_STA                    0x408UL
#define DISP_REG_DPC_DISP1_MTCMOS_STATE_STA              0x40CUL
#define DISP_REG_DPC_DISP1_MTCMOS_OFF_PROT_CFG           0x410UL
#define DISP_REG_DPC_DISP1_DSI_PLL_READY_TIME            0x414UL
#define DISP_REG_DPC_DISP1_THREADx_SET(n)                (0x420UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CLR(n)                (0x440UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DISP1_THREADx_CFG(n)                (0x460UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_MTCMOS_CFG                     0x500UL
#define DISP_REG_DPC_OVL0_MTCMOS_ON_DELAY_CFG            0x504UL
#define DISP_REG_DPC_OVL0_MTCMOS_STA                     0x508UL
#define DISP_REG_DPC_OVL0_MTCMOS_STATE_STA               0x50CUL
#define DISP_REG_DPC_OVL0_MTCMOS_OFF_PROT_CFG            0x510UL
#define DISP_REG_DPC_OVL0_THREADx_SET(n)                 (0x520UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CLR(n)                 (0x540UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL0_THREADx_CFG(n)                 (0x560UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_MTCMOS_CFG                     0x600UL
#define DISP_REG_DPC_OVL1_MTCMOS_ON_DELAY_CFG            0x604UL
#define DISP_REG_DPC_OVL1_MTCMOS_STA                     0x608UL
#define DISP_REG_DPC_OVL1_MTCMOS_STATE_STA               0x60CUL
#define DISP_REG_DPC_OVL1_MTCMOS_OFF_PROT_CFG            0x610UL
#define DISP_REG_DPC_OVL1_THREADx_SET(n)                 (0x620UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CLR(n)                 (0x640UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_OVL1_THREADx_CFG(n)                 (0x660UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_MTCMOS_CFG                     0x700UL
#define DISP_REG_DPC_MML1_MTCMOS_ON_DELAY_CFG            0x704UL
#define DISP_REG_DPC_MML1_MTCMOS_STA                     0x708UL
#define DISP_REG_DPC_MML1_MTCMOS_STATE_STA               0x70CUL
#define DISP_REG_DPC_MML1_MTCMOS_OFF_PROT_CFG            0x710UL
#define DISP_REG_DPC_MML1_THREADx_SET(n)                 (0x720UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CLR(n)                 (0x740UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_MML1_THREADx_CFG(n)                 (0x760UL + 0x4 * (n))	// n = 0 ~ 7
#define DISP_REG_DPC_DUMMY0                              0x800UL
#define DISP_REG_DPC_HW_SEMA0                            0x804UL
#define DISP_REG_DPC_DUMMY1                              0x808UL
#define DISP_REG_DPC_DT_STA0                             0x810UL	// TE Trigger
#define DISP_REG_DPC_DT_STA1                             0x814UL	// DSI SOF Trigger
#define DISP_REG_DPC_DT_STA2                             0x818UL	// DSI Frame Done Trigger
#define DISP_REG_DPC_DT_STA3                             0x81CUL	// Frame Done + Read Done
#define DISP_REG_DPC_POWER_STATE_STATUS                  0x820UL
#define DISP_REG_DPC_MTCMOS_STATUS                       0x824UL
#define DISP_REG_DPC_MTCMOS_CHECK_STATUS                 0x828UL
#define DISP_REG_DPC_DISP0_DEBUG0                        0x840UL
#define DISP_REG_DPC_DISP0_DEBUG1                        0x844UL
#define DISP_REG_DPC_DISP1_DEBUG0                        0x848UL
#define DISP_REG_DPC_DISP1_DEBUG1                        0x84CUL
#define DISP_REG_DPC_OVL0_DEBUG0                         0x850UL
#define DISP_REG_DPC_OVL0_DEBUG1                         0x854UL
#define DISP_REG_DPC_OVL1_DEBUG0                         0x858UL
#define DISP_REG_DPC_OVL1_DEBUG1                         0x85CUL
#define DISP_REG_DPC_MML1_DEBUG0                         0x860UL
#define DISP_REG_DPC_MML1_DEBUG1                         0x864UL
#define DISP_REG_DPC_DT_DEBUG0                           0x868UL
#define DISP_REG_DPC_DT_DEBUG1                           0x86CUL
#define DISP_REG_DPC_DEBUG_SEL                           0x870UL
#define DISP_REG_DPC_DEBUG_STA                           0x874UL

#define DISP_REG_DPC_DISP_DT_EN                          0x0D8UL //  0 ~ 31 : BIT0 ~ BIT31
#define DISP_REG_DPC_MML_DT_EN                           0x0E0UL // 32 ~ 56 : BIT0 ~ BIT24
#define DISP_REG_DPC2_DISP_DT_EN                         0x0F0UL // 57 ~ 65 : BIT0 ~ BIT8
#define DISP_REG_DPC2_MML_DT_EN                          0x0F4UL // 66 ~ 74 : BIT0 ~ BIT8
#define DISP_REG_DPC2_DISP_DT_EN                         0x0F0UL // 75 ~ 77 : BIT9 ~ BIT11

#define DISP_REG_DPC_DISP_DT_SW_TRIG_EN                  0x0DCUL //  0 ~ 31 : BIT0 ~ BIT31
#define DISP_REG_DPC_MML_DT_SW_TRIG_EN                   0x0E4UL // 32 ~ 56 : BIT0 ~ BIT24
#define DISP_REG_DPC2_DISP_DT_SW_TRIG_EN                 0x0F8UL // 57 ~ 65 : BIT0 ~ BIT8
#define DISP_REG_DPC2_MML_DT_SW_TRIG_EN                  0x0FCUL // 66 ~ 74 : BIT0 ~ BIT8
#define DISP_REG_DPC2_DISP_DT_SW_TRIG_EN                 0x0F8UL // 75 ~ 77 : BIT9 ~ BIT11
#define DISP_REG_DPC2_DTx_SW_TRIG(n)                     (0x300UL + 0x4 * (n))	// n = 0 ~ 77

#define DISP_DPC2_DISP_26M_PMIC_VCORE_OFF_CFG            0xA08UL
#define DISP_DPC2_MML_26M_PMIC_VCORE_OFF_CFG             0xA0CUL
#define DISP_DPC_MIPI_SODI5_EN                           0xE00UL
#define DISP_DPC_ON2SOF_DT_EN                            0xE40UL
#define DISP_DPC_ON2SOF_DSI0_SOF_COUNTER                 0xE48UL

/* V3 only begin */
#define DISP_DPC_DISP_DT_TE_MON_SEL_G0                   0x038UL
#define DISP_DPC_DISP_DT_TE_MON_SEL_G1                   0x03CUL
#define DISP_DPC_MML_DT_TE_MON_SEL_G0                    0x040UL
#define DISP_DPC_MML_DT_TE_MON_SEL_G1                    0x044UL
#define DISP_DPC_MMINFRA_HWVOTE_CFG                      0x0B0UL

#define DISP_REG_DPC3_DTx_SW_TRIG(n)                     (0x300UL + 0x4 * (n))	// n = 0 ~ 80

#define DISP_SW_OFF_CONFIG_PADDR_W_PWRITE                0x6A0UL
#define DISP_SW_OFF_CONFIG_PWRITE                        0x6A4UL

#define DISP_DPC_EVENT_SEL_G0                            0x500UL
#define DISP_DPC_EVENT_SEL_G1                            0x504UL
#define DISP_DPC_EVENT_SEL_G2                            0x508UL
#define DISP_DPC_EVENT_SEL_G3                            0x50CUL
#define DISP_DPC_EVENT_SEL_G4                            0x510UL
#define DISP_DPC_EVENT_SEL_G5                            0x514UL
#define DISP_DPC_EVENT_SEL_G6                            0x518UL
#define DISP_DPC_EVENT_SEL_G7                            0x51CUL
#define DISP_DPC_EVENT_CFG                               0x520UL
#define DISP_DPC_INTEN_MTCMOS_ON_OFF                     0x600UL
#define DISP_DPC_INTEN_HWVOTE_STATE                      0x604UL
#define DISP_DPC_INTEN_MTCMOS_BUSY                       0x608UL
#define DISP_DPC_INTEN_INTF_PWR_RDY_STATE                0x60CUL
#define DISP_DPC_INTEN_DISP_PM_CFG_ERROR                 0x610UL
#define DISP_DPC_INTEN_DISP_DT_ERROR                     0x614UL
#define DISP_DPC_INTEN_DT_TE_THREAD                      0x618UL
#define DISP_DPC_INTEN_MML_STATE                         0x61CUL
#define DISP_DPC_INTEN_MML_ERROR                         0x620UL
#define DISP_DPC_INTSTA_MTCMOS_ON_OFF                    0x700UL
#define DISP_DPC_INTSTA_HWVOTE_STATE                     0x704UL
#define DISP_DPC_INTSTA_MTCMOS_BUSY                      0x708UL
#define DISP_DPC_INTSTA_INTF_PWR_RDY_STATE               0x70CUL
#define DISP_DPC_INTSTA_DISP_PM_CFG_ERROR                0x710UL
#define DISP_DPC_INTSTA_DISP_DT_ERROR                    0x714UL
#define DISP_DPC_INTSTA_DT_TE_THREAD                     0x718UL
#define DISP_DPC_INTSTA_MML_STATE                        0x71CUL
#define DISP_DPC_INTSTA_MML_ERROR                        0x720UL
#define DISP_DPC_MTCMOS_STATUS                           0x824UL

/* DPC monitor */
#define DISP_DPC_DDREN_MONITOR_CFG                       0x860UL

#define DISP_DPC_DISPSYS_DRAM_TOTAL_HIGH_BW              0xAA0UL
#define DISP_DPC_DISPSYS_EMI_TOTAL_HIGH_BW               0xAA4UL
#define DISP_DPC_DISPSYS_DRAM_TOTAL_LOW_BW               0xAA8UL
#define DISP_DPC_DISPSYS_EMI_TOTAL_LOW_BW                0xAACUL
#define DISP_DPC_MMLSYS_DRAM_TOTAL_BW                    0xAB0UL
#define DISP_DPC_MMLSYS_EMI_TOTAL_BW                     0xAB4UL

#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE7            BIT(31)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE6_0          BIT(30)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE5            BIT(29)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE4            BIT(28)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE3            BIT(27)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE2_0          BIT(26)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE1            BIT(25)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DT_DONE0            BIT(24)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_PERI_MTCMOS_OFF     BIT(23)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DPTX_MTCMOS_OFF     BIT(22)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML2_MTCMOS_OFF          BIT(21)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML1_MTCMOS_OFF          BIT(20)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML0_MTCMOS_OFF          BIT(19)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL2_MTCMOS_OFF          BIT(18)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL1_MTCMOS_OFF          BIT(17)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL0_MTCMOS_OFF          BIT(16)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1B_MTCMOS_OFF        BIT(15)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_OFF        BIT(14)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP0B_MTCMOS_OFF        BIT(13)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP0A_MTCMOS_OFF        BIT(12)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_PERI_MTCMOS_ON      BIT(11)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP_DPTX_MTCMOS_ON      BIT(10)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML2_MTCMOS_ON           BIT(9)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML1_MTCMOS_ON           BIT(8)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_MML0_MTCMOS_ON           BIT(7)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL2_MTCMOS_ON           BIT(6)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL1_MTCMOS_ON           BIT(5)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_OVL0_MTCMOS_ON           BIT(4)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1B_MTCMOS_ON         BIT(3)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP1A_MTCMOS_ON         BIT(2)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP0B_MTCMOS_ON         BIT(1)
#define INTEN_MTCMOS_ON_OFF_FLD_INTEN_DISP0A_MTCMOS_ON         BIT(0)

#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_7 BIT(31)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_6 BIT(30)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_5 BIT(29)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_4 BIT(28)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_3 BIT(27)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_2 BIT(26)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_1 BIT(25)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_RESERVE_DISP_PM_CFG_ERROR_0 BIT(24)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP_PERI_PWR_OFF_CONFIG BIT(23)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP_DPTX_PWR_OFF_CONFIG BIT(22)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML2_PWR_OFF_CONFIG BIT(21)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML1_PWR_OFF_CONFIG BIT(20)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML0_PWR_OFF_CONFIG BIT(19)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL2_PWR_OFF_CONFIG BIT(18)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL1_PWR_OFF_CONFIG BIT(17)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL0_PWR_OFF_CONFIG BIT(16)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP1B_PWR_OFF_CONFIG BIT(15)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP1A_PWR_OFF_CONFIG BIT(14)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP0B_PWR_OFF_CONFIG BIT(13)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP0A_PWR_OFF_CONFIG BIT(12)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP_PERI_PM_TIME_OUT BIT(11)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP_DPTX_PM_TIME_OUT BIT(10)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML2_PM_TIME_OUT BIT(9)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML1_PM_TIME_OUT BIT(8)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_MML0_PM_TIME_OUT BIT(7)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL2_PM_TIME_OUT BIT(6)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL1_PM_TIME_OUT BIT(5)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_OVL0_PM_TIME_OUT BIT(4)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP1B_PM_TIME_OUT BIT(3)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP1A_PM_TIME_OUT BIT(2)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP0B_PM_TIME_OUT BIT(1)
#define INTEN_DISP_PM_CFG_ERROR_FLD_INTEN_ERR_DISP0A_PM_TIME_OUT BIT(0)

#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_PERI_THREAD_FALLING  BIT(31)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DPTX_THREAD_FALLING  BIT(30)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML2_THREAD_FALLING       BIT(29)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML1_THREAD_FALLING       BIT(28)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML0_THREAD_FALLING       BIT(27)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL2_THREAD_FALLING       BIT(26)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL1_THREAD_FALLING       BIT(25)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL0_THREAD_FALLING       BIT(24)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP1B_THREAD_FALLING     BIT(23)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP1A_THREAD_FALLING     BIT(22)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP0B_THREAD_FALLING     BIT(21)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP0A_THREAD_FALLING     BIT(20)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_PERI_THREAD_RISING   BIT(19)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DPTX_THREAD_RISING   BIT(18)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML2_THREAD_RISING        BIT(17)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML1_THREAD_RISING        BIT(16)
#define INTEN_DT_TE_THREAD_FLD_INTEN_MML0_THREAD_RISING        BIT(15)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL2_THREAD_RISING        BIT(14)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL1_THREAD_RISING        BIT(13)
#define INTEN_DT_TE_THREAD_FLD_INTEN_OVL0_THREAD_RISING        BIT(12)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP1B_THREAD_RISING      BIT(11)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP1A_THREAD_RISING      BIT(10)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP0B_THREAD_RISING      BIT(9)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP0A_THREAD_RISING      BIT(8)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_7          BIT(7)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_6          BIT(6)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_5          BIT(5)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_4          BIT(4)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_3          BIT(3)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_2          BIT(2)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_1          BIT(1)
#define INTEN_DT_TE_THREAD_FLD_INTEN_DISP_DT_TE_SEL_0          BIT(0)

#define INTSTA_DISP0A_HWVOTE_SIGN_HI     BIT(0)
#define INTSTA_DISP0B_HWVOTE_SIGN_HI     BIT(1)
#define INTSTA_DISP1A_HWVOTE_SIGN_HI     BIT(2)
#define INTSTA_DISP1B_HWVOTE_SIGN_HI     BIT(3)
#define INTSTA_OVL0_HWVOTE_SIGN_HI       BIT(4)
#define INTSTA_OVL1_HWVOTE_SIGN_HI       BIT(5)
#define INTSTA_OVL2_HWVOTE_SIGN_HI       BIT(6)
#define INTSTA_MML0_HWVOTE_SIGN_HI       BIT(7)
#define INTSTA_MML1_HWVOTE_SIGN_HI       BIT(8)
#define INTSTA_MML2_HWVOTE_SIGN_HI       BIT(9)
#define INTSTA_DISP0A_HWVOTE_SIGN_LO     BIT(12)
#define INTSTA_DISP0B_HWVOTE_SIGN_LO     BIT(13)
#define INTSTA_DISP1A_HWVOTE_SIGN_LO     BIT(14)
#define INTSTA_DISP1B_HWVOTE_SIGN_LO     BIT(15)
#define INTSTA_OVL0_HWVOTE_SIGN_LO       BIT(16)
#define INTSTA_OVL1_HWVOTE_SIGN_LO       BIT(17)
#define INTSTA_OVL2_HWVOTE_SIGN_LO       BIT(18)
#define INTSTA_MML0_HWVOTE_SIGN_LO       BIT(19)
#define INTSTA_MML1_HWVOTE_SIGN_LO       BIT(20)
#define INTSTA_MML2_HWVOTE_SIGN_LO       BIT(21)
/* V3 only end */

#define DISP_DPC_INT_DISP1_ON                            BIT(31)
#define DISP_DPC_INT_DISP1_OFF                           BIT(30)
#define DISP_DPC_INT_DISP0_ON                            BIT(29)
#define DISP_DPC_INT_DISP0_OFF                           BIT(28)
#define DISP_DPC_INT_OVL1_ON                             BIT(27)
#define DISP_DPC_INT_OVL1_OFF                            BIT(26)
#define DISP_DPC_INT_OVL0_ON                             BIT(25)
#define DISP_DPC_INT_OVL0_OFF                            BIT(24)
#define DISP_DPC_INT_DT31                                BIT(23)
#define DISP_DPC_INT_DT30                                BIT(22)
#define DISP_DPC_INT_DT29                                BIT(21)
#define DISP_DPC_INT_DSI_DONE                            BIT(20)
#define DISP_DPC_INT_DSI_START                           BIT(19)
#define DISP_DPC_INT_DT_TRIG_FRAME_DONE                  BIT(18)
#define DISP_DPC_INT_DT_TRIG_SOF                         BIT(17)
#define DISP_DPC_INT_DT_TRIG_TE                          BIT(16)
#define DISP_DPC_INT_INFRA_OFF_END                       BIT(15)
#define DISP_DPC_INT_INFRA_OFF_START                     BIT(14)
#define DISP_DPC_INT_MMINFRA_OFF_END                     BIT(13)
#define DISP_DPC_INT_MMINFRA_OFF_START                   BIT(12)
#define DISP_DPC_INT_DISP1_ACK_TIMEOUT                   BIT(11)
#define DISP_DPC_INT_DISP0_ACK_TIMEOUT                   BIT(10)
#define DISP_DPC_INT_OVL1_ACK_TIMEOUT                    BIT(9)
#define DISP_DPC_INT_OVL0_ACK_TIMEOUT                    BIT(8)
#define DISP_DPC_INT_DT7                                 BIT(7)
#define DISP_DPC_INT_DT6                                 BIT(6)
#define DISP_DPC_INT_DT5                                 BIT(5)
#define DISP_DPC_INT_DT4                                 BIT(4)
#define DISP_DPC_INT_DT3                                 BIT(3)
#define DISP_DPC_INT_DT2                                 BIT(2)
#define DISP_DPC_INT_DT1                                 BIT(1)
#define DISP_DPC_INT_DT0                                 BIT(0)

#define DISP_DPC_VDO_MODE                                BIT(16)
#define DISP_DPC_MMQOS_ALWAYS_SCAN_EN                    BIT(4)
#define DISP_DPC_DT_EN                                   BIT(1)
#define DISP_DPC_EN                                      BIT(0)

#define VOTE_SET 1
#define VOTE_CLR 0

#define DPC_DISP_DT_CNT 32
#define DPC_MML_DT_CNT 25

enum mtk_dpc_sp_type {
	DPC_SP_TE,
	DPC_SP_SOF,
	DPC_SP_FRAME_DONE,
	DPC_SP_RROT_DONE,
};

enum mtk_dpc_cap_id {
	DPC_CAP_MTCMOS,
	DPC_CAP_APSRC,
	DPC_CAP_VDISP,
	DPC_CAP_QOS,
	DPC_CAP_MMINFRA_PLL,
	DPC_CAP_PMIC_VCORE,
	DPC_CAP_DSI,
	DPC_CAP_CNT
};
#define has_cap(id) (g_priv && (g_priv->vidle_mask & BIT(id)))

enum mtk_dpc_bw_type {
	DPC_TOTAL_HRT,
	DPC_TOTAL_SRT,
	DPC_BW_TYPE_CNT,
};

struct mtk_dpc_dt_usage {
	s16 index;
	enum mtk_dpc_sp_type sp;		/* start point */
	u16 ep;					/* end point in us */
	u16 group;
};

struct mtk_dpc2_dt_usage {
	u8 en;
	u32 val;
};

struct mtk_dpc_dvfs_bw {
	u32 mml_bw;
	u32 disp_bw[DPC_BW_TYPE_CNT];
	u32 mml0_bw[DPC_BW_TYPE_CNT];
	u32 mml1_bw[DPC_BW_TYPE_CNT];
	u32 mml2_bw[DPC_BW_TYPE_CNT];
	u8 bw_level;
	u8 mml_level;
	u8 disp_level;
	struct mutex lock;
};

struct mtk_dpc_mtcmos_cfg {
	u16 cfg;
	u16 thread_set;
	u16 thread_clr;
	resource_size_t chk_pa;
	void __iomem *chk_va;
	enum mtk_dpc_mtcmos_mode mode;
	u8 link_bit;
};

struct mtk_dpc_channel_bw_cfg {
	u16 offset;
	u8 shift;
	u16 disp_bw;
	u16 mml_bw;
};

static void mtk_disp_vlp_vote_v2(unsigned int vote_set, unsigned int thread);
static void dpc_dt_set_v2(u16 dt, u32 counter);
static void dpc_mtcmos_vote_v3(const u32 subsys, const u8 thread, const bool en);
static void dpc_ch_bw_set_v2(const u32 subsys, const u8 idx, const u32 bw_in_mb);
static void dpc_dvfs_set_v2(const u32 subsys, const u8 level, bool update_level);
static bool dpc_is_power_on_v2(void);
static bool mminfra_is_power_on_v2(void);
static u8 bw_to_level_v3(const u32 total_bw);
static void dpc_analysis_v2(void);
static void dpc_hwccf_vote(bool on, struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user, bool lock,
			   const u16 gpr);
static void process_dbg_opt(const char *opt);
static int dpc_vidle_power_keep_v3(const enum mtk_vidle_voter_user _user);
static void dpc_vidle_power_release_v3(const enum mtk_vidle_voter_user _user);

struct mtk_dpc {
	struct platform_device *pdev;
	struct device *dev;
	struct device *pd_dev;			/* mminfra mtcmos */
	struct device *root_dev;		/* disp_vcore mtcmos */
	struct notifier_block smi_nb;
	int disp_irq;
	int mml_irq;
	resource_size_t dpc_pa;
	void __iomem *mminfra_hangfree;
	bool enabled;
	bool vcp_is_alive;
	bool skip_force_power;
	spinlock_t skip_force_power_lock;
	spinlock_t mtcmos_cfg_lock;
	spinlock_t hwccf_ref_lock;
	spinlock_t excp_spin_lock;
	struct mutex excp_lock;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *fs;
#endif
	struct mtk_dpc_dvfs_bw dvfs_bw;

	unsigned int mmsys_id;
	unsigned int pwr_clk_num;
	struct clk **pwr_clk;
	u32 vidle_mask;
	u32 vidle_mask_bk;
	u32 dt_follow_cfg;
	u32 total_srt_unit;
	u32 total_hrt_unit;
	u32 srt_emi_efficiency;			/* total srt * srt_emi_efficiency / 100 */
	u32 hrt_emi_efficiency;			/* total hrt * 100 / hrt_emi_efficiency */
	u32 ch_bw_urate;			/* channel bw * 100 / ch_bw_urate */
	u32 dsi_ck_keep_mask;
	u32 mminfra_pwr_idx;
	u32 mminfra_pwr_type;
	u16 event_hwccf_vote;

	void __iomem *vdisp_dvfsrc;
	u32 vdisp_dvfsrc_idle_mask;
	u8 vdisp_level_cnt;

	void __iomem *dispvcore_chk;
	u32 dispvcore_chk_mask;

	void __iomem *mminfra_chk;
	u32 mminfra_chk_mask;

	resource_size_t voter_set_pa;
	resource_size_t voter_clr_pa;
	void __iomem *voter_set_va;
	void __iomem *voter_clr_va;
	void __iomem *vcore_mode_set_va;
	void __iomem *vcore_mode_clr_va;

	void __iomem *rtff_pwr_con;
	void __iomem *vdisp_ao_cg_con;
	void __iomem *mminfra_voter;
	void __iomem *mminfra_dummy;

	struct mtk_dpc_mtcmos_cfg *mtcmos_cfg;
	int subsys_cnt;

	struct mtk_dpc_dt_usage *disp_dt_usage;
	struct mtk_dpc_dt_usage *mml_dt_usage;
	struct mtk_dpc2_dt_usage *dpc2_dt_usage;
	struct mtk_dpc_channel_bw_cfg *ch_bw_cfg;

	void (*set_mtcmos)(const u32 subsys, const enum mtk_dpc_mtcmos_mode mode);
	irqreturn_t (*disp_irq_handler)(int irq, void *dev_id);
	irqreturn_t (*mml_irq_handler)(int irq, void *dev_id);
	void (*duration_update)(const u32 us);
	void (*enable)(const u8 en);
	int (*res_init)(struct mtk_dpc *priv);
	void (*hrt_bw_set)(const u32 subsys, const u32 bw_in_mb, bool force);
	void (*srt_bw_set)(const u32 subsys, const u32 bw_in_mb, bool force);
	void (*mtcmos_vote)(const u32 subsys, const u8 thread, const bool en);
	void (*group_enable)(const u16 group, bool en);
	int (*power_keep)(const u32 user);
	void (*power_release)(const u32 user);
	void (*power_keep_by_gce)(struct cmdq_pkt *pkt, const u32 user, const u16 gpr, void *reuse);
	void (*power_release_by_gce)(struct cmdq_pkt *pkt, const u32 user, void *reuse);
	void (*config)(const u32 subsys, bool en);
	void (*analysis)(void);
};

#endif
