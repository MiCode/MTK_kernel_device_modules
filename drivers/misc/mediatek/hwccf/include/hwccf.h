// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _HWCCF_H_
#define _HWCCF_H_
#include <linux/types.h> /* Kernel only */

#define CCF_CG_HISTORY_NUM              (16)    /* Number of CG Transaction record */
#define CCF_XPU_VOTE_HISTORY_NUM        (28)    /* Number of XPU Vote CG history record */
#define CCF_XPU_VOTE_IRQ_HISTORY_NUM    (14)    /* Number of XPU Vote IRQ history record */
#define CCF_TIMELINE_HISTORY_NUM        (40)    /* Number of Timeline history record */

#define u32_all_MASK (0xFFFFFFFF)

#define IS_SET_FROM_VOTER_ADDR(ofs)  (((ofs) & 0x7ff) < 0x700 ? \
					(((ofs) & 0x7ff) % 0xc) == 0 : \
					(((ofs) & 0x7ff) % 0xc) == 4)
#define GET_ID_FROM_SET_ADDR(ofs) \
	((((ofs) & 0xfff) <= 0x264) ? (((ofs) & 0xfff) / 0xc) : \
	(((ofs) & 0xfff) == 0x600) ? 54 : \
	(((ofs) & 0xfff) == 0x700) ? 52 : \
	(((ofs) & 0xfff) == 0x70c) ? 53 : 0xffffffff)

/* XPU local Voter */
#define CCF_OFS(ofs)  (ofs)
#define CCF_XPU(n) ((n < 5) ?  (0x20000 + (n * 0x10000)) : \
							   (0x70000 + ((n - 5) * 0x1000)))

#define CCF_XPU_CG_SET(n, x)       CCF_OFS(CCF_XPU(n)         + (x) * 0xc)
#define CCF_XPU_CG_CLR(n, x)       CCF_OFS(CCF_XPU(n) + 0x004 + (x) * 0xc)
#define CCF_XPU_CG_EN(n, x)        CCF_OFS(CCF_XPU(n) + 0x008 + (x) * 0xc)
#define CCF_XPU_MUX_SEL_SET(n, x)  CCF_OFS(CCF_XPU(n) + 0x300 + \
	((x) / MUXES_PER_VOTER) * 0xc)
#define CCF_XPU_MUX_SEL_CLR(n, x)  CCF_OFS(CCF_XPU(n) + 0x304 + \
	((x) / MUXES_PER_VOTER) * 0xc)
#define CCF_XPU_MUX_SEL_EN(n, x)   CCF_OFS(CCF_XPU(n) + 0x308 + \
	((x) / MUXES_PER_VOTER) * 0xc)
#define CCF_XPU_MUX_SEL_UPDATE(n)  CCF_OFS(CCF_XPU(n) + 0x500)
#define CCF_XPU_MUX_PWR_SET(n)     CCF_OFS(CCF_XPU(n) + 0x600)
#define CCF_XPU_MUX_PWR_CLR(n)     CCF_OFS(CCF_XPU(n) + 0x604)
#define CCF_XPU_MUX_PWR_EN(n)      CCF_OFS(CCF_XPU(n) + 0x608)
#define CCF_XPU_MTCMOS0_SET(n)     CCF_OFS(CCF_XPU(n) + 0x700)
#define CCF_XPU_MTCMOS0_CLR(n)     CCF_OFS(CCF_XPU(n) + 0x704)
#define CCF_XPU_MTCMOS0_EN(n)      CCF_OFS(CCF_XPU(n) + 0x708)
#define CCF_XPU_MTCMOS1_SET(n)     CCF_OFS(CCF_XPU(n) + 0x70c)
#define CCF_XPU_MTCMOS1_CLR(n)     CCF_OFS(CCF_XPU(n) + 0x710)
#define CCF_XPU_MTCMOS1_EN(n)      CCF_OFS(CCF_XPU(n) + 0x714)
#define CCF_XPU_B1_SET(n)          CCF_OFS(CCF_XPU(n) + 0x724)
#define CCF_XPU_B1_CLR(n)          CCF_OFS(CCF_XPU(n) + 0x728)
#define CCF_XPU_B1_EN(n)           CCF_OFS(CCF_XPU(n) + 0x72c)
#define CCF_XPU_B2_SET(n)          CCF_OFS(CCF_XPU(n) + 0x730)
#define CCF_XPU_B2_CLR(n)          CCF_OFS(CCF_XPU(n) + 0x734)
#define CCF_XPU_B2_EN(n)           CCF_OFS(CCF_XPU(n) + 0x738)
#define CCF_XPU_B3_SET(n)          CCF_OFS(CCF_XPU(n) + 0x73c)
#define CCF_XPU_B3_CLR(n)          CCF_OFS(CCF_XPU(n) + 0x740)
#define CCF_XPU_B3_EN(n)           CCF_OFS(CCF_XPU(n) + 0x744)

/* Utilities for HWCCF Voter */
#define CG_VOTER_ID_FROM_OFS(ofs)    (((ofs) & 0x1ff) / 0xc)

/* HWCCF FSM state */
#define CCF_STATUS     CCF_OFS(0x10000)
enum CCF_STATUS_BIT {
	hwccf_idle                   = (1u << 0),
};

/* VIP Global Status/Done/Enable */
#define CCF_OUTPUT_STATUS     CCF_OFS(0x10004)
enum CCF_OUTPUT_STATUS_BIT {
	/* all hwccf FSMs are idle or not */
	hw_ccf_idle_o                = (1u << 26),
	/* interrupt event to ap */
	hw_ccf_to_mcu_int_b          = (1u << 27),
	/* interrupt event to vip */
	hw_ccf_to_up_int_b_0         = (1u << 28),
	hw_ccf_to_up_int_b_1         = (1u << 29),
	hw_ccf_to_up_int_b_2         = (1u << 30),
	hw_ccf_to_up_int_b_3         = (1u << 31),
};
#define CCF_FSM_REQ_ACK	      CCF_OFS(0x10008)
// CG VIP Status
#define CCF_VIP_CG_STA(x)     CCF_OFS(0x11800 + (x) * 0x4)
#define CCF_VIP_CG_EN(x)      CCF_OFS(0x12200 + (x) * 0x4)
#define CCF_VIP_CG_DONE(x)    CCF_OFS(0x12600 + (x) * 0x4)
// MUX on/off XPU polling status
#define CCF_MUX_PWR_STA           CCF_OFS(0x12030)
#define CCF_MUX_PWR_EN            CCF_OFS(0x12330)
#define CCF_MUX_PWR_DONE          CCF_OFS(0x12730)
#define CCF_IRQ_MUX_PWR_EN        CCF_OFS(0x13430)
#define CCF_IRQ_MUX_PWR_STA       CCF_OFS(0x13530)
#define CCF_IRQ_MUX_PWR_DONE      CCF_OFS(0x13630)
#define CCF_ALL_MUX_EN            CCF_OFS(0x14104)
#define CCF_ALL_MUX_STA           CCF_OFS(0x14114)
// MUX dfs status
#define CCF_IRQ_MUX_SEL_STA(i)    CCF_OFS(0x13500 + (i) * 0x4)
#define CCF_IRQ_MUX_SEL_EN(i)     CCF_OFS(0x13400 + (i) * 0x4)
#define CCF_IRQ_MUX_SEL_DONE(i)   CCF_OFS(0x13600 + (i) * 0x4)
// MTCMOS VIP Status
#define CCF_MTCMOS_EN_0       CCF_OFS(0x11318)
#define CCF_MTCMOS_STA_0      CCF_OFS(0x1131c)
#define CCF_MTCMOS_DONE_0     CCF_OFS(0x1132c)
#define CCF_MTCMOS_EN_1       CCF_OFS(0x11330)
#define CCF_MTCMOS_STA_1      CCF_OFS(0x11334)
#define CCF_MTCMOS_DONE_1     CCF_OFS(0x11344)
#define CCF_ALL_MTCMOS_EN_0   CCF_OFS(0x13700)
#define CCF_ALL_MTCMOS_STA_0  CCF_OFS(0x13704)
#define CCF_ALL_MTCMOS_EN_1   CCF_OFS(0x13708)
#define CCF_ALL_MTCMOS_STA_1  CCF_OFS(0x1370C)
#define CCF_MTCMOS_IRQ_EN_0   CCF_OFS(0x13800)
#define CCF_MTCMOS_IRQ_STA_0  CCF_OFS(0x13804)
#define CCF_MTCMOS_IRQ_EN_1   CCF_OFS(0x13808)
#define CCF_MTCMOS_IRQ_STA_1  CCF_OFS(0x1380c)
// Backup VIP Status
#define CCF_BACKUP3_EN        CCF_OFS(0x11300)
#define CCF_BACKUP3_STA       CCF_OFS(0x11304)
#define CCF_BACKUP3_DONE      CCF_OFS(0x11314)
#define CCF_BACKUP1_EN        CCF_OFS(0x11358)
#define CCF_BACKUP1_STA       CCF_OFS(0x1135c)
#define CCF_BACKUP1_DONE      CCF_OFS(0x1136c)
#define CCF_BACKUP2_EN        CCF_OFS(0x11370)
#define CCF_BACKUP2_STA       CCF_OFS(0x11374)
#define CCF_BACKUP2_DONE      CCF_OFS(0x11384)

/* VIP IRQ STATUS CLEAR */
#define CCF_MTCMOS_VIP_STA_CLR_0    CCF_OFS(0x4)
#define CCF_MTCMOS_VIP_STA_CLR_1    CCF_OFS(0x8)
#define CCF_BACKUP1_VIP_STA_CLR     CCF_OFS(0x10)
#define CCF_BACKUP2_VIP_STA_CLR     CCF_OFS(0x14)
#define CCF_BACKUP3_VIP_STA_CLR     CCF_OFS(0x18)
#define CCF_VIP_MUX_PWR_STA_CLR     CCF_OFS(0x1900)
#define CCF_VIP_MUX_SEL_STA_CLR(i)  CCF_OFS(0x190c + (i) * 0x4)

#define CCF_MASK_HW_MTCMOS_REQ_0    CCF_OFS(0x5854)
#define CCF_MASK_HW_IRQ_REQ_0       CCF_OFS(0x5858)

// PM REQ & ACK offset
#define CCF_MTCMOS_PM_ACK_0         CCF_OFS(0x12900)
#define CCF_MTCMOS_PM_ACK_1         CCF_OFS(0x12904)

/* IRQ Status (vip w1c) */
#define CCF_INT_STATUS                 CCF_OFS(0x50)
enum CCF_INT_STA_BIT {
	cg_fsm_timeout_int_to_vip          = (1u << 0),
	mux_sel_int_to_vip                 = (1u << 1),
	backup3_timeout_int_to_vip         = (1u << 2),
	mtcmos0_timeout_int_to_vip         = (1u << 3),
	mtcmos1_timeout_int_to_vip         = (1u << 4),
	backup1_timeout_int_to_vip         = (1u << 5),
	backup2_timeout_int_to_vip         = (1u << 6),
	cg_map_mtcmos_violation_to_vip     = (1u << 7),
	mtcmos_fsm_timeout_to_vip          = (1u << 8),
	vote_before_ready_int_to_vip       = (1u << 9),
	mux_fsm_timeout_to_vip             = (1u << 10),
	cg_int_to_vip                      = (1u << 11),
	mux_sel_irq_timeout_int_to_vip     = (1u << 12),
	mtcmos_int_to_vip                  = (1u << 13),
	backup_vote_int_to_vip             = (1u << 14),
	mux_pdn_int_to_vip                 = (1u << 15),
	/* interrupt event to ap */
	buck_int_to_ap                     = (1u << 16),
	mux_sel_int_to_ap                  = (1u << 17),
	backup3_timeout_int_to_ap          = (1u << 18),
	mtcmos0_timeout_int_to_ap          = (1u << 19),
	mtcmos1_timeout_int_to_ap          = (1u << 20),
	backup1_timeout_int_to_ap          = (1u << 21),
	backup2_timeout_int_to_ap          = (1u << 22),
	cg_map_mtcmos_violation_to_ap      = (1u << 23),
	mtcmos_fsm_timeout_to_ap           = (1u << 24),
	vote_before_ready_int_to_ap        = (1u << 25),
	mux_fsm_timeout_to_ap              = (1u << 26),
	cg_int_to_ap                       = (1u << 27),
	mux_sel_irq_timeout_int_to_ap      = (1u << 28),
	mtcmos_int_to_ap                   = (1u << 29),
	backup_vote_int_to_ap              = (1u << 30),
	mux_pdn_int_to_ap                  = (1u << 31),
};

// Register_VIP_CLR
#define CCF_INT_VOTE_VIO          CCF_OFS(0x54)
enum CCF_INT_VOTE_VIO_BIT {
	xpu_vote_before_ready_vio_flag  = (1u << 1),
};

// Timeout Configuration
#define CCF_CG_FSM_TIMEOUT         CCF_OFS(0x64)
#define CCF_MTCMOS_FSM_TIMEOUT     CCF_OFS(0x6c)
#define CCF_MUX_FSM_TIMEOUT        CCF_OFS(0x78) /* for on/off */
#define CCF_IRQ_TIMEOUT            CCF_OFS(0x90)
	#define TIMEOUTCFG_EN_BIT      (1u << 0)
	#define BUS_MAX_FREQ_MHZ       (130)
	#define TIMEOUTCFG_1MS         (0x1F90010 | TIMEOUTCFG_EN_BIT)
	#define TIMEOUTCFG_MIN         (0x10002   | TIMEOUTCFG_EN_BIT)
	#define TIMEOUTCFG_MAX         (0x3FF003E | TIMEOUTCFG_EN_BIT)
	#define TO_CNT_TARGET_BIT      (16)
	#define TO_CNT_TARGET_MSK      (0x3ff)
	#define TO_DIV_CNT_TARGET_BIT  (1)
	#define TO_DIV_CNT_TARGET_MSK  (0x1f)

/* Initial Config (xpu r/o, vip r/w) */
#define CCF_CTRL            CCF_OFS(0x150C)
enum CCF_CTRL_BIT {
	hw_ccf_cg_en                            = (1u << 0),
	dis_cg_map_mtcmos_violation_int         = (1u << 1),
	dis_hw_ccf_mtcmos_map_mux_violation     = (1u << 2),
	dis_vote_before_ready_violation_int     = (1u << 3),
	reg_vip_skip_en_vote                    = (1u << 5),

	en_cg_int_for_vip                       = (1u << 7),
	dis_pll_int_for_vip                     = (1u << 8),
	dis_cg_timeout_int0_for_vip             = (1u << 9),
	dis_irq_timeout_for_vip                 = (1u << 11),
	dis_map_violation_for_vip               = (1u << 12),
	dis_mtcmos_fsm_timeout_int_for_vip      = (1u << 13),
	dis_vote_before_ready_violation_for_vip = (1u << 14),
	dis_mux_fsm_timeout_int_for_vip         = (1u << 15),
	dis_mtcmos_int_for_vip                  = (1u << 16),
	dis_backup_int_for_vip                  = (1u << 17),

	en_cg_int_for_ap                        = (1u << 18),
	dis_pll_int_for_ap                      = (1u << 19),
	dis_cg_timeout_int0_for_ap              = (1u << 20),
	dis_irq_timeout_for_ap                  = (1u << 22),
	dis_map_violation_for_ap                = (1u << 23),
	dis_mtcmos_fsm_timeout_int_for_ap       = (1u << 24),
	dis_vote_before_ready_vio_for_ap        = (1u << 25),
	dis_mux_fsm_timeout_int_for_ap          = (1u << 26),
	dis_mtcmos_int_for_ap                   = (1u << 27),
	dis_backup_int_for_ap                   = (1u << 28),

	dis_all_int_to_vip                      = (1u << 30),
	dis_all_int_to_ap                       = (1u << 31),
};

#define CCF_DCM             CCF_OFS(0x1510)
	#define CCF_DCM_ENABLE_CFG  0x7
	#define DCM_DBC_TIME_BIT    4
#define CCF_REG_EN0         CCF_OFS(0x1514)
enum CCF_REG_EN0_BIT {
	reg_hw_ccf_vip_psel          = (0xfffu << 0),
	reg_hw_ccf_debug_psel        = (0xfffu << 13),
	reg_en_cksys_fsm             = (1u << 27),
	reg_mtcmos_fsm_hier_en       = (1u << 28),
	reg_mtcmos_pm_en             = (1u << 29),
	reg_en_clr_addr_map          = (1u << 30),
	reg_en_mtcmos_fsm            = (1u << 31)
};
#define CCF_REG_EN1         CCF_OFS(0x1518)
#define CCF_REG_EN2         CCF_OFS(0x151C)
#define CCF_REG_EN3         CCF_OFS(0x1520)
#define CCF_REG_EN4         CCF_OFS(0x1524)
#define CCF_REG_EN5         CCF_OFS(0x1528)
#define HWCCF_REG_EN6       CCF_OFS(0x152C)
enum CCF_REG_EN6_BIT {
	cg_read_xpu0_for_other       = (1u << 0),
	cg_read_xpu1_for_other       = (1u << 1),
	cg_read_xpu2_for_other       = (1u << 2),
	cg_read_xpu3_for_other       = (1u << 3),
	cg_read_xpu4_for_other       = (1u << 4),
	cg_read_xpu5_for_other       = (1u << 5),
	cg_read_xpu6_for_other       = (1u << 6),
	cg_read_xpu7_for_other       = (1u << 7),
	cg_read_xpu8_for_other       = (1u << 8),
	cg_read_xpu9_for_other       = (1u << 9),
	pll_mtcmos_read_for_other    = (1u << 10),
	config_read_for_other        = (1u << 11),
	cg_status_read_for_other     = (1u << 12),
	xpu_mask_vote_read_for_other = (1u << 13),
	pre_vote_mux_mask_out        = (1u << 14),
	reg_vote_dis_wi_vio_config   = (1u << 15),
	block_voting                 = (1u << 16),
};
#define CCF_APB_M_REG0      CCF_OFS(0x1530)
enum CCF_APB_M_REG0_BIT {
	en_apb_m_his                = (1u << 1),
	en_apb_m                    = (1u << 2),
	manu_en_hw_ccf_paddr_m      = (1u << 3),
	manu_en_hw_ccf_pwdata_m     = (1u << 9),
	apb_m_addr_sft              = (1u << 13),
	apb_m_addr_3bit             = (0x7u << 14),
	en_mon_special              = (1u << 18),
	apb_m_debug_index           = (0x7u << 19),
	en_mux_check                = (1u << 23),
	en_xpu_vote_cg_his          = (1u << 24),
	en_xpu_vote_irq_his         = (1u << 25),
};
#define CCF_APB_M_REG1      CCF_OFS(0x1534)
#define CCF_APB_M_REG2      CCF_OFS(0x1538)
#define CCF_APB_M_REG3      CCF_OFS(0x153C)
#define CCF_APB_M_REG4      CCF_OFS(0x1540)
#define CCF_APB_M_REG5      CCF_OFS(0x1544)
enum CCF_APB_M_REG5_BIT {
	enable_vote_bit = 1u << 0,
};
#define CCF_APB_M_REG6      CCF_OFS(0x1548)
enum CCF_APB_M_REG6_BIT {
	dis_mux_int_for_vip         = (1u << 16),
	dis_mux_int_for_mcu         = (1u << 17),
	dis_mux_pdn_int_for_vip     = (1u << 18),
	dis_mux_pdn_int_for_mcu     = (1u << 19),
};

/* Autolink CG/MTCMOS/MUX Settings */
#define CCF_MTCMOS_HW_MODE_SEL_0        CCF_OFS(0x1570)
#define CCF_MTCMOS_HW_MODE_SEL_1        CCF_OFS(0x1574)
#define CCF_FSM_REQ_ASK_MASK            CCF_OFS(0x1578)
enum CCF_FSM_REQ_ASK_MASK_BIT {
	mtcmos_fsm_mask_cg_req      = (1u << 0),
	mtcmos_fsm_mask_cg_ack      = (1u << 1),
	mtcmos_fsm_mask_mux_req     = (1u << 2),
	mtcmos_fsm_mask_mux_ack     = (1u << 3),
};
#define CCF_MUX_HW_MODE_SEL_0           CCF_OFS(0x1580)
/* CG-MTCMOS Mapping, 0x15A0~15Bc */
#define INIT_CCF_CG_MAP_MTCMOS(cg)          CCF_OFS(0x15A0 + (cg)*0x4)
	#define CG_MTCMOS_FILED_WIDTH           7
	#define CG_MTCMOS_FIELD_COUNT           4
	#define CG_MTCMOS_FIELD_MASK            (0x7f)
/* CG-Address Mapping, 0x1600~16EC */
#define INIT_CCF_CG_SET_ADDR(x)             CCF_OFS(0x1600 + (x) * 0x8)
#define INIT_CCF_CG_CLR_ADDR(x)             CCF_OFS(0x1604 + (x) * 0x8)
/* MUX-CG Mapping, 0x4000~40f8 */
#define INIT_CCF_MUX_MAP_CG(mux, cg) CCF_OFS(0x4000 + (mux) * 8 + (cg / 32) * 4)
/* MTCMOS-MUX Mapping, 0x5108~53fc */
#define INIT_CCF_MTCMOS_MAP_MUX(mtc)        CCF_OFS(0x5108 + (mtc) * 0xc)
/* MTCMOS-MTCMOS Mapping, 0x5608~5644 */
#define INIT_CCF_MTCMOS_MAP_MTCMOS(x)       CCF_OFS(0x5608 + (x) * 0x4)
	#define MTCMOS_MTCMOS_FILED_WIDTH       (8)
	#define MTCMOS_MTCMOS_FIELD_COUNT       (4)
	#define MTCMOS_MTCMOS_FIELD_MASK        ((1U << MTCMOS_MTCMOS_FILED_WIDTH) - 1)
	#define MTCMOS_LEVEL_SHIFT              (6)
	#define MTCMOS_LEVEL_MASK               (0x3)
	#define MTCMOS_MAP_SHIFT                (0)
	#define MTCMOS_MAP_MASK                 ((1U << MTCMOS_LEVEL_SHIFT) - 1)

enum MTCMOS_HIERARCHY_LV {
	MTCMOS_LV0, /* lowest child */
	MTCMOS_LV1,
	MTCMOS_LV2,
	MTCMOS_LV3, /* highest parent */
	MTCMOS_LV_NUM,
};

/* Violation Log (vip r/o & w1c) */
#define CCF_INT_CG_MAP_VIO_0         CCF_OFS(0x1988)
#define CCF_INT_CG_MAP_VIO_1         CCF_OFS(0x198c)
#define CCF_CG_MTCMOS_VIO_GP         CCF_OFS(0x1273c)
#define CCF_CG_MTCMOS_VIO_DATA(x)    CCF_OFS(0x12740 + (x)*0x4)
#define CCF_CG_MTCMOS_VIO_ADDR(x)    CCF_OFS(0x12754 + (x)*0x4)
#define CCF_CG_MAP_VIO_DATA(x)       CCF_OFS(0x12800 + (x)*0x4)
#define CCF_CG_MAP_VIO_ADDR(x)       CCF_OFS(0x12814 + (x)*0x4)
#define CCF_VOTE_EN_VIO_DATA         CCF_OFS(0x12828)
#define CCF_VOTE_EN_VIO_ADDR         CCF_OFS(0x1282c)

/* Timeout Debug */
#define CCF_IRQ_TIMEOUT_DBG            CCF_OFS(0x128c4)
#define CCF_CG_FSM_TIMEOUT_DBG         CCF_OFS(0x14200)
#define CCF_CG_FSM_TIMEOUT0_DBG0       CCF_OFS(0x14204)
#define CCF_MTCMOS_FSM_TIMEOUT_DBG     CCF_OFS(0x14208)
#define CCF_MTCMOS_FSM_TIMEOUT_DBG0    CCF_OFS(0x1420C)
#define CCF_MTCMOS_FSM_TIMEOUT_DBG1    CCF_OFS(0x14210)
#define CCF_MUX_FSM_TIMEOUT_DBG        CCF_OFS(0x14214)
#define CCF_MUX_FSM_TIMEOUT_DBG0       CCF_OFS(0x14218)
enum HWCCF_TIMEOUT_TYPE {
	CCF_CG_TIMEOUT
};

/* Rester_REDUMDENT */
#define CCF_MTCMOS_FLAG_SET     CCF_OFS(0x5648)
#define CCF_MTCMOS_FLAG_CLR     CCF_OFS(0x564C)
#define HWCCF_ECO_OPEN_0        CCF_OFS(0x14240)
#define HWCCF_ECO_OPEN_1        CCF_OFS(0x14244)
#define HWCCF_ECO_XPU_0         CCF_OFS(0x14248)
#define HWCCF_ECO_XPU_1         CCF_OFS(0x1424c)
	#define HWCCF_INIT_DONE_REG (HWCCF_ECO_XPU_0)
#define HWCCF_ECO_VIP_0         CCF_OFS(0x3FA8)
#define HWCCF_ECO_VIP_1         CCF_OFS(0x3FAC)
#define HW_CCF_DUMMY_SET_0      CCF_OFS(0x3FB0)
#define HW_CCF_DUMMY_CLR_0      CCF_OFS(0x3FB4)
#define HW_CCF_DUMMY_ENABLE_0   CCF_OFS(0x3FB8)
#define HWCCF_XPU_ECO(n)        CCF_OFS(CCF_XPU(n) + 0x808)

/* CG Transaction History */
#define CCF_APB_M_DBG0              CCF_OFS(0x12838)
	#define CCF_DEBUG_HISTORY(x)    (CCF_APB_M_DBG0 + (x) * 0x4)
#define CCF_ADDR_LOG(x)             CCF_OFS(0x1283c + (x) * 0x4)
#define CCF_DATA_LOG(x)             CCF_OFS(0x1287c + (x) * 0x4)
#define CCF_INDEX_PTR               CCF_OFS(0x128bc)

/* hwccf timeline history */
#define CCF_INPUT_TIMELINE_PTR           CCF_OFS(0x12438)
	#define CCF_TIMELINE_INDEX_BITS      (5)
	#define CCF_TIMELINE_IDX_MSK         (0x1f)
	#define CCF_TIMELINE_BITS            (CCF_TIMELINE_INDEX_BITS + 1)
	#define CCF_TIMELINE_MSK             (0x3f)
	#define CCF_TIMELINE_LOG_PER_ENTERY  (32 / CCF_TIMELINE_BITS)
#define CCF_INPUT_TIMELINE_LOG_ADDR(x)   CCF_OFS(0x1243c + ((x / CCF_TIMELINE_LOG_PER_ENTERY) * 4))
	#define CCF_TIMELINE_SHIFT(x)        ((x % CCF_TIMELINE_LOG_PER_ENTERY) * CCF_TIMELINE_BITS)
	#define CCF_TIMELINE_GET_VAL(val, x) ((val >> CCF_TIMELINE_SHIFT(x)) & CCF_TIMELINE_MSK)
enum CCF_INPUT_TIMELINE_TYPE {
	CCF_INPUT_TIMELINE_TYPE_CG,
	CCF_INPUT_TIMELINE_TYPE_IRQ,
	CCF_INPUT_TIMELINE_TYPE_MAX,
};
/* XPU CG voter history */
#define CCF_CG_HIS_INDEX_PTR         CCF_OFS(0x128d0)
#define CCF_XPU_VOTE_ADDR_LOG(x)     CCF_OFS(0x12100 + (x) * 0x4)
#define CCF_XPU_VOTE_DATA_LOG(x)     CCF_OFS(0x1245c + (x) * 0x4)
/* IRQ voter history */
#define CCF_VOTE_IRQ_HIS_INDEX_PTR      CCF_OFS(0x124cc)
#define CCF_XPU_VOTE_IRQ_ADDR_LOG(x)    CCF_OFS(0x124d0 + (x)*0x4)
#define CCF_XPU_VOTE_IRQ_DATA_LOG(x)    CCF_OFS(0x12508 + (x)*0x4)

#ifdef HWCCF_INIT_FLOW
enum HWCCF_TEST_FLAG {
	HWCCF_NORMAL, /* Normal Usage, Not in test mode */
	HWCCF_TIMEOUT_TEST,
	HWCCF_VIO_TEST,
};

/* Mapping HWCCF MUX ID to CLK Data MUX Id */
struct mux_map {
	uint16_t clk_id;
	uint8_t mux_idx;
};

/* (deprecated) Mapping HWCCF voter with XPU Domain Id */
struct xpu_map {
	uint8_t id;
	uint8_t shift;
};

/* Mapping HWCCF MTCMOS voter to HWCCF MUX voter */
struct mtcmos_mux_map {
	uint8_t mtcmos_idx;
	uint8_t mux_parents[3];
	size_t mux_count;
};
#define MTCMOS_MUX_MAP(mtcmos, ...) { \
	.mtcmos_idx = (mtcmos), \
	.mux_parents = {__VA_ARGS__}, \
	.mux_count = sizeof((uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t) \
}

/* Mapping HWCCF CG voter to HWCCF MUX voter */
struct cg_mux_map {
	uint8_t cg_idx;
	uint8_t mux_parents[3];
	size_t mux_count;
};
#define CG_MUX_MAP(cg, ...) { \
	.cg_idx = (cg), \
	.mux_parents = {__VA_ARGS__}, \
	.mux_count = sizeof((uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t) \
}

/* Mapping HWCCF MTCMOS voter to MTCMOS voter */
struct mtcmos_map {
	uint8_t child_motcmos_id;
	uint8_t child_motcmos_lv;
	uint8_t parent_mtcmos_id;
};

/* Mapping HWCCF CG voter to MTCMOS voter */
struct cg_mtcmos_map {
	uint8_t child_cg;
	uint8_t parent_mtcmos;
};
#define CG_MTCMOS_MAP(cg, mtcmos) { \
	.child_cg = (cg), \
	.parent_mtcmos = (mtcmos), \
}

/* Mapping HWCCF CG voter to physical CG address */
struct voter_map {
	uint32_t map_base;
	uint16_t set_addr;
	uint16_t clr_addr;
	uint16_t sta_addr;
	int8_t id;
	bool polar;
};
#define VOTER_MAP(_id, _base, _set_val, _clr_val, _sta_val, _polar) {	\
		.id = _id,				\
		.map_base = _base,			\
		.set_addr = _set_val,		\
		.clr_addr = _clr_val,		\
		.sta_addr = _sta_val,		\
		.polar = _polar,		\
	}
#endif

#define HWV_XPU_0    (0)
#define HWV_XPU_1    (1)
#define HWV_XPU_2    (2)
#define HWV_XPU_3    (3)
#define HWV_XPU_4    (4)
#define HWV_XPU_5    (5)
#define HWV_XPU_6    (6)
#define HWV_XPU_7    (7)
#define HWV_XPU_8    (8)
#define HWV_XPU_9    (9)

#define HWV_CG_0     (0)
#define HWV_CG_1     (1)
#define HWV_CG_2     (2)
#define HWV_CG_3     (3)
#define HWV_CG_4     (4)
#define HWV_CG_5     (5)
#define HWV_CG_6     (6)
#define HWV_CG_7     (7)
#define HWV_CG_8     (8)
#define HWV_CG_9     (9)
#define HWV_CG_10    (10)
#define HWV_CG_11    (11)
#define HWV_CG_12    (12)
#define HWV_CG_13    (13)
#define HWV_CG_14    (14)
#define HWV_CG_15    (15)
#define HWV_CG_16    (16)
#define HWV_CG_17    (17)
#define HWV_CG_18    (18)
#define HWV_CG_19    (19)
#define HWV_CG_20    (20)
#define HWV_CG_21    (21)
#define HWV_CG_22    (22)
#define HWV_CG_23    (23)
#define HWV_CG_24    (24)
#define HWV_CG_25    (25)
#define HWV_CG_26    (26)
#define HWV_CG_27    (27)
#define HWV_CG_28    (28)
#define HWV_CG_29    (29)
#define HWV_CG_30    (30)
#define HWV_CG_31    (31)
#define HWV_CG_32    (32)
#define HWV_CG_33    (33)
#define HWV_CG_34    (34)
#define HWV_CG_35    (35)
#define HWV_CG_36    (36)
#define HWV_CG_37    (37)
#define HWV_CG_38    (38)
#define HWV_CG_39    (39)
#define HWV_CG_40    (40)
#define HWV_CG_41    (41)
#define HWV_CG_42    (42)
#define HWV_CG_43    (43)
#define HWV_CG_44    (44)
#define HWV_CG_45    (45)
#define HWV_CG_46    (46)
#define HWV_CG_47    (47)
#define HWV_CG_48    (48)
#define HWV_CG_49    (49)
#define HWV_CG_50    (50)
#define HWV_CG_51    (51)
#define HWV_CG_52    (52)
#define HWV_CG_53    (53)
#define HWV_CG_54    (54)
#define HWV_CG_55    (55)
#define HWV_CG_56    (56)
#define HWV_CG_57    (57)
#define HWV_CG_58    (58)
#define HWV_CG_59    (59)
#define HWV_CG_60    (60)
#define HWV_CG_61    (61)
#define HWV_CG_62    (62)
#define HWV_CG_63    (63)

#define HWV_PM_NULL  (CCF_MTCMOS_NUM)
#define HWV_PM_0     (0)
#define HWV_PM_1     (1)
#define HWV_PM_2     (2)
#define HWV_PM_3     (3)
#define HWV_PM_4     (4)
#define HWV_PM_5     (5)
#define HWV_PM_6     (6)
#define HWV_PM_7     (7)
#define HWV_PM_8     (8)
#define HWV_PM_9     (9)
#define HWV_PM_10    (10)
#define HWV_PM_11    (11)
#define HWV_PM_12    (12)
#define HWV_PM_13    (13)
#define HWV_PM_14    (14)
#define HWV_PM_15    (15)
#define HWV_PM_16    (16)
#define HWV_PM_17    (17)
#define HWV_PM_18    (18)
#define HWV_PM_19    (19)
#define HWV_PM_20    (20)
#define HWV_PM_21    (21)
#define HWV_PM_22    (22)
#define HWV_PM_23    (23)
#define HWV_PM_24    (24)
#define HWV_PM_25    (25)
#define HWV_PM_26    (26)
#define HWV_PM_27    (27)
#define HWV_PM_28    (28)
#define HWV_PM_29    (29)
#define HWV_PM_30    (30)
#define HWV_PM_31    (31)
#define HWV_PM_32    (32)
#define HWV_PM_33    (33)
#define HWV_PM_34    (34)
#define HWV_PM_35    (35)
#define HWV_PM_36    (36)
#define HWV_PM_37    (37)
#define HWV_PM_38    (38)
#define HWV_PM_39    (39)
#define HWV_PM_40    (40)
#define HWV_PM_41    (41)
#define HWV_PM_42    (42)
#define HWV_PM_43    (43)
#define HWV_PM_44    (44)
#define HWV_PM_45    (45)
#define HWV_PM_46    (46)
#define HWV_PM_47    (47)
#define HWV_PM_48    (48)
#define HWV_PM_49    (49)
#define HWV_PM_50    (50)
#define HWV_PM_51    (51)
#define HWV_PM_52    (52)
#define HWV_PM_53    (53)
#define HWV_PM_54    (54)
#define HWV_PM_55    (55)
#define HWV_PM_56    (56)
#define HWV_PM_57    (57)
#define HWV_PM_58    (58)
#define HWV_PM_59    (59)
#define HWV_PM_60    (60)
#define HWV_PM_61    (61)
#define HWV_PM_62    (62)
#define HWV_PM_63    (63)

#define HWV_MUX_0     (0)
#define HWV_MUX_1     (1)
#define HWV_MUX_2     (2)
#define HWV_MUX_3     (3)
#define HWV_MUX_4     (4)
#define HWV_MUX_5     (5)
#define HWV_MUX_6     (6)
#define HWV_MUX_7     (7)
#define HWV_MUX_8     (8)
#define HWV_MUX_9     (9)
#define HWV_MUX_10    (10)
#define HWV_MUX_11    (11)
#define HWV_MUX_12    (12)
#define HWV_MUX_13    (13)
#define HWV_MUX_14    (14)
#define HWV_MUX_15    (15)
#define HWV_MUX_16    (16)
#define HWV_MUX_17    (17)
#define HWV_MUX_18    (18)
#define HWV_MUX_19    (19)
#define HWV_MUX_20    (20)
#define HWV_MUX_21    (21)
#define HWV_MUX_22    (22)
#define HWV_MUX_23    (23)
#define HWV_MUX_24    (24)
#define HWV_MUX_25    (25)
#define HWV_MUX_26    (26)
#define HWV_MUX_27    (27)
#define HWV_MUX_28    (28)
#define HWV_MUX_29    (29)
#define HWV_MUX_30    (30)
#define HWV_MUX_31    (31)

/* Wrapper AP voter */
/* CG set/clr id x to ofs */
#define CG_SET_OFS(x)       CCF_XPU_CG_SET(HWV_XPU_0, x)
#define CG_CLR_OFS(x)       CCF_XPU_CG_CLR(HWV_XPU_0, x)
#define CG_DONE_OFS(x)      CCF_VIP_CG_DONE(x)
#define CG_STA_OFS(x)       CCF_VIP_CG_STA(x)
#define CG_GLB_EN_OFS(x)    CCF_VIP_CG_EN(x)

/* MUX PWR ON set/clr to ofs */
#define MUX_PWR_SET_OFS     CCF_XPU_MUX_PWR_SET(HWV_XPU_0)
#define MUX_PWR_CLR_OFS     CCF_XPU_MUX_PWR_CLR(HWV_XPU_0)
#define MUX_PWR_DONE_OFS    CCF_MUX_PWR_DONE
#define MUX_PWR_STA_OFS     CCF_MUX_PWR_STA
#define MUX_PWR_GLB_EN_OFS  CCF_MUX_PWR_EN

/* MTCMOS id to ofs */
#define MTCMOS0_SET_OFS     CCF_XPU_MTCMOS0_SET(HWV_XPU_0)
#define MTCMOS0_CLR_OFS     CCF_XPU_MTCMOS0_CLR(HWV_XPU_0)
#define MTCMOS0_DONE_OFS    CCF_MTCMOS_DONE_0
#define MTCMOS0_STA_OFS     CCF_MTCMOS_STA_0
#define MTCMOS0_GLB_EN_OFS  CCF_MTCMOS_EN_0

#define MTCMOS1_SET_OFS     CCF_XPU_MTCMOS1_SET(HWV_XPU_0)
#define MTCMOS1_CLR_OFS     CCF_XPU_MTCMOS1_CLR(HWV_XPU_0)
#define MTCMOS1_DONE_OFS    CCF_MTCMOS_DONE_1
#define MTCMOS1_STA_OFS     CCF_MTCMOS_STA_1
#define MTCMOS1_GLB_EN_OFS  CCF_MTCMOS_EN_1

#define XPU_B0_SET          CCF_XPU_B1_SET(HWV_XPU_0)
#define XPU_B0_CLR          CCF_XPU_B1_CLR(HWV_XPU_0)
#define XPU_B0_DONE         CCF_BACKUP1_DONE
#define XPU_B0_STA          CCF_BACKUP1_STA
#define XPU_B0_GLB_EN       CCF_BACKUP1_EN

#define XPU_B1_SET          CCF_XPU_B2_SET(HWV_XPU_0)
#define XPU_B1_CLR          CCF_XPU_B2_CLR(HWV_XPU_0)
#define XPU_B1_DONE         CCF_BACKUP2_DONE
#define XPU_B1_STA          CCF_BACKUP2_STA
#define XPU_B1_GLB_EN       CCF_BACKUP2_EN

#define XPU_B2_SET          CCF_XPU_B3_SET(HWV_XPU_0)
#define XPU_B2_CLR          CCF_XPU_B3_CLR(HWV_XPU_0)
#define XPU_B2_DONE         CCF_BACKUP3_DONE
#define XPU_B2_STA          CCF_BACKUP3_STA
#define XPU_B2_GLB_EN       CCF_BACKUP3_EN

#endif /* _HWCCF_H_ */
