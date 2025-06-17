/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_H__
#define __MTK_DPC_H__

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

/* NOTE: user 0 to 7 is reserved for genpd notifier enum disp_pd_id { ... } */
enum mtk_vidle_voter_user {
	DISP_VIDLE_USER_DISP_VCORE = 0,
	DISP_VIDLE_USER_NST_LOCK = 9,
	DISP_VIDLE_USER_PQ2 = 10,
	DISP_VIDLE_USER_MML_CLK_ISR = 11,
	DISP_VIDLE_USER_MML2 = 12,
	DISP_VIDLE_USER_MML2_CMDQ = 13,
	DISP_VIDLE_USER_FOR_FRAME = 14,
	DISP_VIDLE_USER_TOP_CLK_ISR = 15,
	DISP_VIDLE_USER_CRTC = 16,
	DISP_VIDLE_USER_PQ = 17,
	DISP_VIDLE_USER_MML = 18,
	DISP_VIDLE_USER_MML1 = DISP_VIDLE_USER_MML,
	DISP_VIDLE_USER_MDP = 19,
	DISP_VIDLE_USER_MML0 = DISP_VIDLE_USER_MDP,
	DISP_VIDLE_USER_DISP_CMDQ = 20,
	DISP_VIDLE_USER_DDIC_CMDQ = 21,
	DISP_VIDLE_USER_PQ_CMDQ = 22,
	DISP_VIDLE_USER_TRIGLOOP_CMDQ = 23,
	DISP_VIDLE_USER_MML_CMDQ = 24,
	DISP_VIDLE_USER_MML1_CMDQ = DISP_VIDLE_USER_MML_CMDQ,
	DISP_VIDLE_USER_MML0_CMDQ = 25,
	DISP_VIDLE_USER_DISP_DPC_CFG = 26,
	DISP_VIDLE_USER_MML0_DPC_CFG = 27,
	DISP_VIDLE_USER_MML1_DPC_CFG = 28,
	DISP_VIDLE_USER_DPC_DUMP = 29,
	DISP_VIDLE_USER_SMI_DUMP = 30,
	DISP_VIDLE_FORCE_KEEP = 31,
	DISP_VIDLE_USER_MASK = 0x1f,
};

enum mtk_vidle_voter_status {
	VOTER_PM_FAILED = -1,
	VOTER_PM_DONE = 0,
	VOTER_PM_LATER,
	VOTER_PM_SKIP_PWR_OFF,
	VOTER_ONLY = 0x1000,
};

enum mtk_dpc_mtcmos_mode {
	DPC_MTCMOS_MANUAL,
	DPC_MTCMOS_AUTO,
};

enum mtk_panel_type {
	PANEL_TYPE_CMD,
	PANEL_TYPE_VDO,
	PANEL_TYPE_COUNT,
};

enum mtk_dpc_version {
	DPC_VER_UNKNOWN,
	DPC_VER1,
	DPC_VER2,
	DPC_VER3,
	DPC_VER_CNT,
};

enum mtk_dpc_subsys {
	DPC_SUBSYS_DISP = 0,
	DPC_SUBSYS_DIS0 = 0,
	DPC_SUBSYS_DIS1 = 1,
	DPC_SUBSYS_OVL0 = 2,
	DPC_SUBSYS_OVL1 = 3,
	DPC_SUBSYS_MML = 4,
	DPC_SUBSYS_MML2 = 4,
	DPC_SUBSYS_MML1 = 4,
	DPC_SUBSYS_MML0 = 5,
	DPC_SUBSYS_EDP = 6,
	DPC_SUBSYS_DPTX = 7,
	DPC_SUBSYS_CNT
};

enum mtk_dpc_subsys_v3 {
	DPC3_SUBSYS_DIS0A,
	DPC3_SUBSYS_DIS0B,
	DPC3_SUBSYS_DIS1A,
	DPC3_SUBSYS_DIS1B,
	DPC3_SUBSYS_OVL0,
	DPC3_SUBSYS_OVL1,
	DPC3_SUBSYS_OVL2,
	DPC3_SUBSYS_MML0,
	DPC3_SUBSYS_MML1,
	DPC3_SUBSYS_MML2,
	DPC3_SUBSYS_DPTX,
	DPC3_SUBSYS_PERI,
	DPC3_SUBSYS_CNT,
	DPC3_SUBSYS_DISP_DDIC = 0xc,	/* disp1ab */
	DPC3_SUBSYS_DISP_PQ = 0xf,	/* disp0ab disp1ab */
	DPC3_SUBSYS_DISP = 0x7f,	/* disp0ab disp1ab ovl012 */
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

enum mtk_dpc2_disp_vidle {
	DPC2_DISP_VIDLE_MTCMOS = 0,
	DPC2_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC2_DISP_VIDLE_VDISP_DVFS = 8,
	DPC2_DISP_VIDLE_HRT_BW = 11,
	DPC2_DISP_VIDLE_SRT_BW = 14,
	DPC2_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC2_DISP_VIDLE_INFRA_OFF = 20,
	DPC2_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC2_DISP_VIDLE_MSYNC2_0 = 26,
	DPC2_DISP_VIDLE_RESERVED = 29,
	DPC2_MML_VIDLE_MTCMOS = 32,
	DPC2_MML_VIDLE_VDISP_DVFS = 36,
	DPC2_MML_VIDLE_HRT_BW = 39,
	DPC2_MML_VIDLE_SRT_BW = 42,
	DPC2_MML_VIDLE_MMINFRA_OFF = 45,
	DPC2_MML_VIDLE_INFRA_OFF = 48,
	DPC2_MML_VIDLE_MAINPLL_OFF = 51,
	DPC2_MML_VIDLE_RESERVED = 54,
	DPC2_DISP_VIDLE_26M = 57,
	DPC2_DISP_VIDLE_PMIC = 60,
	DPC2_DISP_VIDLE_VCORE = 63,
	DPC2_MML_VIDLE_26M = 66,
	DPC2_MML_VIDLE_PMIC = 69,
	DPC2_MML_VIDLE_VCORE = 72,
	DPC2_DISP_VIDLE_DSIPHY = 75,
	DPC2_VIDLE_CNT = 78
};

enum mtk_dpc3_disp_vidle {
	DPC3_DISP_VIDLE_MTCMOS = 0,
	DPC3_DISP_VIDLE_MTCMOS_DISP1 = 4,
	DPC3_DISP_VIDLE_VDISP_DVFS = 8,
	DPC3_DISP_VIDLE_HRT_BW = 11,
	DPC3_DISP_VIDLE_SRT_BW = 14,
	DPC3_DISP_VIDLE_MMINFRA_OFF = 17,
	DPC3_DISP_VIDLE_INFRA_OFF = 20,
	DPC3_DISP_VIDLE_MAINPLL_OFF = 23,
	DPC3_DISP_VIDLE_MSYNC2_0 = 26,
	DPC3_DISP_VIDLE_RESERVED = 29,
	DPC3_MML_VIDLE_MTCMOS = 32,
	DPC3_MML_VIDLE_VDISP_DVFS = 36,
	DPC3_MML_VIDLE_HRT_BW = 39,
	DPC3_MML_VIDLE_SRT_BW = 42,
	DPC3_MML_VIDLE_MMINFRA_OFF = 45,
	DPC3_MML_VIDLE_INFRA_OFF = 48,
	DPC3_MML_VIDLE_MAINPLL_OFF = 51,
	DPC3_MML_VIDLE_RESERVED = 54,
	DPC3_DISP_VIDLE_26M = 57,
	DPC3_DISP_VIDLE_PMIC = 60,
	DPC3_DISP_VIDLE_VCORE = 63,
	DPC3_MML_VIDLE_26M = 66,
	DPC3_MML_VIDLE_PMIC = 69,
	DPC3_MML_VIDLE_VCORE = 72,
	DPC3_DISP_VIDLE_DSIPHY = 75,
	DPC3_DISP_VIDLE_PERI = 78,
	DPC3_VIDLE_CNT = 81
};

struct dpc_funcs {
	/* only for display driver */
	void (*dpc_enable)(const u8 en);
	void (*dpc_duration_update)(u32 us);

	/* dpc driver internal use */
	void (*dpc_ddr_force_enable)(const u32 subsys, const bool en);

	/* resource auto mode control */
	void (*dpc_group_enable)(const u16 group, bool en);

	/* mtcmos auto mode control */
	void (*dpc_mtcmos_auto)(const u32 subsys, const enum mtk_dpc_mtcmos_mode mode);

	/* mtcmos and resource auto mode control */
	void (*dpc_pause)(const u32 subsys, bool en);
	void (*dpc_config)(const u32 subsys, bool en);

	/* exception power control */
	int (*dpc_vidle_power_keep)(const enum mtk_vidle_voter_user);
	void (*dpc_vidle_power_release)(const enum mtk_vidle_voter_user);
	void (*dpc_vidle_power_keep_by_gce)(struct cmdq_pkt *pkt,
					    const enum mtk_vidle_voter_user user, const u16 gpr,
					    void *reuse);
	void (*dpc_vidle_power_release_by_gce)(struct cmdq_pkt *pkt,
					       const enum mtk_vidle_voter_user user,
					       void *reuse);

	/* dpc monitor */
	void (*dpc_monitor_config)(struct cmdq_pkt *cmdq_handle, const u32 value);

	/* dram dvfs
	 * @bw_in_mb: [U32_MAX]: read stored bw, otherwise update stored bw
	 * @force:    [TRUE]: trigger dvfs immediately
	 */
	void (*dpc_hrt_bw_set)(const u32 subsys, const u32 bw_in_mb, bool force);
	void (*dpc_srt_bw_set)(const u32 subsys, const u32 bw_in_mb, bool force);

	/* check dpc vdisp level and pll mux, [0]: the same, [level]: dpc setting */
	u8 (*dpc_check_pll)(void);

	/* vdisp dvfs
	 * @update_level: [TRUE]: update stored level, [FALSE]: only trigger dvfs
	 */
	void (*dpc_dvfs_set)(const u32 subsys, const u8 level, bool update_level);

	/* mminfra dvfs */
	void (*dpc_channel_bw_set_by_idx)(const u32 subsys, const u8 idx, const u32 bw_in_mb);

	/* trigger all dvfs immediately */
	void (*dpc_dvfs_trigger)(const char *caller);

	void (*dpc_clear_wfe_event)(struct cmdq_pkt *pkt, enum mtk_vidle_voter_user user, int event);
	void (*dpc_mtcmos_vote)(const u32 subsys, const u8 thread, const bool en);
	void (*dpc_analysis)(void);
	void (*dpc_debug_cmd)(const char *opt);
	void (*dpc_mtcmos_on_off)(bool on, struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user, bool lock,
				  const u16 gpr);
	int (*dpc_mminfra_on_off)(bool en, const enum mtk_vidle_voter_user user);
	int (*dpc_buck_status)(int op);
	void (*dpc_pre_cg_ctrl)(bool en, bool lock);
	void (*dpc_power_clean_up_by_gce)(struct cmdq_pkt *pkt);

	/* V1 ONLY */
	void (*dpc_dc_force_enable)(const bool en);
	void (*dpc_init_panel_type)(enum mtk_panel_type);
	void (*dpc_dsi_pll_set)(const u32 value);
	int (*dpc_dt_set_all)(u32 dur_frame, u32 dur_vblank);
	void (*dpc_infra_force_enable)(const u32 subsys, const bool en);
	void (*dpc_dvfs_bw_set)(const u32 subsys, const u32 bw_in_mb);
	void (*dpc_dvfs_both_set)(const u32 subsys, const u8 level, bool force,
		const u32 bw_in_mb);
};

#endif
