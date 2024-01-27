/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_H__
#define __MTK_DPC_H__

/*
 * EOF                                                      TE
 *  | OFF0 |                                     | SAFEZONE | OFF1 |
 *  |      |         OVL OFF                     |          |      | OVL OFF |
 *  |      |<-100->| DISP1 OFF           |<-100->|          |      | <-100-> | DISP1 OFF
 *  |      |<-100->| MMINFRA OFF |<-800->|       |          |      | <-100-> | MMINFRA OFF
 *  |      |       |             |       |       |          |      |         |
 *  |      OFF     OFF           ON      ON      ON         |      OFF       OFF
 *         0       4,11          12      5       1                 3         7,13
 */

#define DT_TE_60 16000
#define DT_TE_120 8000
#define DT_TE_360 2650
#define DT_TE_SAFEZONE 350
#define DT_OFF0 38000
#define DT_OFF1 500
#define DT_PRE_DISP1_OFF 100
#define DT_POST_DISP1_OFF 100
#define DT_PRE_MMINFRA_OFF 100
#define DT_POST_MMINFRA_OFF 800 /* infra267 + mminfra300 + margin */

#define DT_MMINFRA_OFFSET (DT_TE_SAFEZONE + DT_POST_DISP1_OFF + DT_POST_MMINFRA_OFF)
#define DT_DISP1_OFFSET   (DT_TE_SAFEZONE + DT_POST_DISP1_OFF)
#define DT_OVL_OFFSET     (DT_TE_SAFEZONE)
#define DT_DISP1TE_OFFSET (DT_TE_SAFEZONE + 50)

#define DT_12 (DT_TE_360 - DT_MMINFRA_OFFSET)
#define DT_5  (DT_TE_360 - DT_DISP1_OFFSET)
#define DT_1  (DT_TE_360 - DT_OVL_OFFSET)
#define DT_6  (DT_TE_360 - DT_DISP1TE_OFFSET)
#define DT_3  (DT_OFF1)
#define DT_7  (DT_OFF1 + DT_PRE_DISP1_OFF)
#define DT_13 (DT_OFF1 + DT_PRE_MMINFRA_OFF)

enum mtk_dpc_subsys {
	DPC_SUBSYS_DISP = 0,
	DPC_SUBSYS_DISP0 = 0,
	DPC_SUBSYS_DISP1 = 1,
	DPC_SUBSYS_OVL0 = 2,
	DPC_SUBSYS_OVL1 = 3,
	DPC_SUBSYS_MML = 4,
	DPC_SUBSYS_MML1 = 4,
	DPC_CHECK_DISP_VCORE,
};

/* NOTE: user 0 to 7 is reserved for genpd notifier enum disp_pd_id { ... } */
enum mtk_vidle_voter_user {
	DISP_VIDLE_USER_CRTC = 16,
	DISP_VIDLE_USER_PQ,
	DISP_VIDLE_USER_MML,
	DISP_VIDLE_USER_MDP,
	DISP_VIDLE_USER_DISP_CMDQ,
	DISP_VIDLE_USER_MML_CMDQ = 24,
	DISP_VIDLE_USER_SMI_DUMP = 30,
	DISP_VIDLE_FORCE_KEEP = 31,
};

enum mtk_dpc_disp_vidle {
	DPC_DISP_VIDLE_MTCMOS = 0,
	DPC_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC_DISP_VIDLE_VDISP_DVFS = 8,
	DPC_DISP_VIDLE_HRT_BW = 11,
	DPC_DISP_VIDLE_SRT_BW = 14,
	DPC_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC_DISP_VIDLE_INFRA_OFF = 20,
	DPC_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC_DISP_VIDLE_MSYNC2_0 = 26,
	DPC_DISP_VIDLE_RESERVED = 29,
	DPC_DISP_VIDLE_MAX = 32,
};

enum mtk_dpc_mml_vidle {
	DPC_MML_VIDLE_MTCMOS = 32,
	DPC_MML_VIDLE_VDISP_DVFS = 36,
	DPC_MML_VIDLE_HRT_BW = 39,
	DPC_MML_VIDLE_SRT_BW = 42,
	DPC_MML_VIDLE_MMINFRA_OFF = 45,
	DPC_MML_VIDLE_INFRA_OFF = 48,
	DPC_MML_VIDLE_MAINPLL_OFF = 51,
	DPC_MML_VIDLE_RESERVED = 54,
};

void dpc_enable(bool en);
void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en);
void dpc_infra_force_enable(const enum mtk_dpc_subsys subsys, const bool en);
void dpc_dc_force_enable(const bool en);
void dpc_group_enable(const u16 group, bool en);
void dpc_config(const enum mtk_dpc_subsys subsys, bool en);
void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force);
void dpc_dvfs_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
int dpc_vidle_power_keep(const enum mtk_vidle_voter_user);
void dpc_vidle_power_release(const enum mtk_vidle_voter_user);

struct dpc_funcs {
	void (*dpc_enable)(bool en);
	void (*dpc_ddr_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);
	void (*dpc_infra_force_enable)(const enum mtk_dpc_subsys subsys, const bool en);
	void (*dpc_dc_force_enable)(const bool en);
	void (*dpc_group_enable)(const u16 group, bool en);
	void (*dpc_config)(const enum mtk_dpc_subsys subsys, bool en);
	void (*dpc_dt_set)(u16 dt, u32 us);
	void (*dpc_mtcmos_vote)(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en);
	int (*dpc_vidle_power_keep)(const enum mtk_vidle_voter_user);
	void (*dpc_vidle_power_release)(const enum mtk_vidle_voter_user);
	void (*dpc_hrt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
	void (*dpc_srt_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force);
	void (*dpc_dvfs_set)(const enum mtk_dpc_subsys subsys, const u8 level, bool force);
	void (*dpc_dvfs_bw_set)(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb);
	void (*dpc_analysis)(void);
};

#endif
