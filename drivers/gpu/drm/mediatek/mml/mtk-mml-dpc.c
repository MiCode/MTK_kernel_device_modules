// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#include "mtk-mml-dpc.h"
#include "mtk-mml-core.h"

int mml_dl_dpc = MML_DPC_PKT_VOTE;
module_param(mml_dl_dpc, int, 0644);

static struct dpc_funcs mml_dpc_funcs;
static enum mtk_dpc_version mml_dpc_version;

#define mml_sysid_to_dpc_subsys(sysid)	\
	(sysid == mml_sys_frame ? DPC_SUBSYS_MML1 : DPC_SUBSYS_MML0)
#define mml_sysid_to_dpc_user(sysid)	\
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1 : DISP_VIDLE_USER_MML0)
#define mml_sysid_to_dpc_user_cmdq(sysid)	\
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1_CMDQ : DISP_VIDLE_USER_MML0_CMDQ)
#define mml_sysid_to_dpc_srt_read_idx(sysid)	\
	(sysid == mml_sys_frame ? 8 : (sysid == mml_sys_tile ? 0 : 8))
#define mml_sysid_to_dpc_hrt_read_idx(sysid)	\
	(sysid == mml_sys_frame ? 10 : (sysid == mml_sys_tile ? 2 : 10))


#define mml_sysid_to_dpc_subsys_v3(sysid)	\
	(sysid == mml_sys_tile ? DPC3_SUBSYS_MML0 : \
	(sysid == mml_sys_frame ? DPC3_SUBSYS_MML1 : DPC3_SUBSYS_MML2))
#define mml_sysid_to_dpc_user_v3(sysid)	\
	(sysid == mml_sys_tile ? DISP_VIDLE_USER_MML0 : \
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1 : DISP_VIDLE_USER_MML2))
#define mml_sysid_to_dpc_user_cmdq_v3(sysid)	\
	(sysid == mml_sys_tile ? DISP_VIDLE_USER_MML0_CMDQ : \
	(sysid == mml_sys_frame ? DISP_VIDLE_USER_MML1_CMDQ : DISP_VIDLE_USER_MML2_CMDQ))
#define mml_larb_idx_to_dpc_subsys(larb_idx)	\
	(larb_idx == 0 ? DPC3_SUBSYS_MML0 : (larb_idx == 1 ? DPC3_SUBSYS_MML1 : DPC3_SUBSYS_MML2))
/* larb_idx [larb2, larb3, larb56, larb57] */
#define mml_larb_idx_to_dpc_srt_read_idx(larb_idx)	\
	(larb_idx == 0 ? 4 : (larb_idx == 1 ? 12 : (larb_idx == 2 ? 0 : (larb_idx == 3 ? 8 : 0))))
#define mml_larb_idx_to_dpc_hrt_read_idx(larb_idx)	\
	(larb_idx == 0 ? 6 : (larb_idx == 1 ? 14 : (larb_idx == 2 ? 2 : (larb_idx == 3 ? 10 : 2))))

void mml_dpc_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version)
{
	mml_dpc_funcs = *funcs;
	mml_dpc_version = version;
}
EXPORT_SYMBOL_GPL(mml_dpc_register);

void mml_dpc_group_enable(u32 sysid, bool en)
{
	if (mml_dpc_funcs.dpc_group_enable == NULL) {
		mml_msg_dpc("%s dpc_group_enable not exist", __func__);
		return;
	}

	switch (mml_dpc_version) {
	case DPC_VER1:
	case DPC_VER2:
		mml_dpc_funcs.dpc_group_enable(
			mml_sysid_to_dpc_subsys(sysid), en);
		break;
	case DPC_VER3:
		mml_dpc_funcs.dpc_group_enable(
			mml_sysid_to_dpc_subsys_v3(sysid), en);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}
}

void mml_dpc_mtcmos_auto(u32 sysid, const bool en, const s8 mode)
{
	enum mtk_dpc_mtcmos_mode mtcmos_mode = en ? DPC_MTCMOS_AUTO : DPC_MTCMOS_MANUAL;

	if (mml_dpc_version == DPC_VER2 || mml_dpc_version == DPC_VER3)
		return;

	if (mml_dpc_funcs.dpc_mtcmos_auto == NULL) {
		mml_msg_dpc("%s dpc_mtcmos_auto not exist", __func__);
		return;
	}

	switch (mml_dpc_version) {
	case DPC_VER1:
		mml_dpc_funcs.dpc_mtcmos_auto(
			mml_sysid_to_dpc_subsys(sysid), mtcmos_mode);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}
}

int mml_dpc_power_keep(u32 sysid)
{
	enum mtk_vidle_voter_user user = DISP_VIDLE_USER_MML;

	switch (mml_dpc_version) {
	case DPC_VER1:
	case DPC_VER2:
		user = mml_sysid_to_dpc_user(sysid);
		break;
	case DPC_VER3:
		user = mml_sysid_to_dpc_user_v3(sysid);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}

	if (mml_dpc_funcs.dpc_vidle_power_keep == NULL) {
		mml_msg_dpc("%s dpc_power_keep not exist", __func__);
		return -1;
	}

	mml_msg_dpc("%s exception flow keep sys id %u user %d", __func__, sysid, user);
	return mml_dpc_funcs.dpc_vidle_power_keep(user);
}

void mml_dpc_power_release(u32 sysid)
{
	enum mtk_vidle_voter_user user = DISP_VIDLE_USER_MML;

	switch (mml_dpc_version) {
	case DPC_VER1:
	case DPC_VER2:
		user = mml_sysid_to_dpc_user(sysid);
		break;
	case DPC_VER3:
		user = mml_sysid_to_dpc_user_v3(sysid);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}

	if (mml_dpc_funcs.dpc_vidle_power_release == NULL) {
		mml_msg_dpc("%s dpc_power_release not exist", __func__);
		return;
	}

	mml_msg_dpc("%s exception flow release sys id %u user %d", __func__, sysid, user);
	mml_dpc_funcs.dpc_vidle_power_release(user);
}

void mml_dpc_isr_keep(void)
{
	if (mml_dpc_funcs.dpc_vidle_power_keep)
		mml_dpc_funcs.dpc_vidle_power_keep(DISP_VIDLE_USER_MML_CLK_ISR);
}

void mml_dpc_isr_release(void)
{
	if (mml_dpc_funcs.dpc_vidle_power_release)
		mml_dpc_funcs.dpc_vidle_power_release(DISP_VIDLE_USER_MML_CLK_ISR);
}

int mml_dpc_power_keep_gce(u32 sysid, struct cmdq_pkt *pkt, u16 gpr, struct cmdq_reuse *reuse)
{
	enum mtk_vidle_voter_user user = DISP_VIDLE_USER_MML_CMDQ;

	switch (mml_dpc_version) {
	case DPC_VER1:
	case DPC_VER2:
		user = mml_sysid_to_dpc_user_cmdq(sysid);
		break;
	case DPC_VER3:
		user = mml_sysid_to_dpc_user_cmdq_v3(sysid);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}

	if (!mml_dpc_funcs.dpc_vidle_power_keep_by_gce) {
		mml_msg_dpc("%s dpc_vidle_power_keep_by_gce not exist", __func__);
		return -1;
	}

	mml_msg_dpc("%s exception flow gce user %d keep", __func__, user);

	mml_dpc_funcs.dpc_vidle_power_keep_by_gce(pkt, user, gpr, (void *)reuse);
	return 0;
}

void mml_dpc_power_release_gce(u32 sysid, struct cmdq_pkt *pkt, struct cmdq_reuse *reuse)
{
	enum mtk_vidle_voter_user user = DISP_VIDLE_USER_MML_CMDQ;

	switch (mml_dpc_version) {
	case DPC_VER1:
	case DPC_VER2:
		user = mml_sysid_to_dpc_user_cmdq(sysid);
		break;
	case DPC_VER3:
		user = mml_sysid_to_dpc_user_cmdq_v3(sysid);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}

	if (!mml_dpc_funcs.dpc_vidle_power_release_by_gce) {
		mml_msg_dpc("%s dpc_vidle_power_release_by_gce not exist", __func__);
		return;
	}

	mml_msg_dpc("%s exception flow gce user %d release", __func__, user);

	mml_dpc_funcs.dpc_vidle_power_release_by_gce(pkt, user, (void *)reuse);
}

void mml_dpc_hrt_bw_set(u32 larb_idx, const u32 bw_in_mb, bool force_keep)
{
	if (mml_dpc_funcs.dpc_hrt_bw_set == NULL) {
		mml_msg_dpc("%s dpc_hrt_bw_set not exist", __func__);
		return;
	}

	switch (mml_dpc_version) {
	case DPC_VER1:
		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_hrt_bw_set(
				mml_sysid_to_dpc_subsys(larb_idx),
				bw_in_mb, force_keep);
		break;
	case DPC_VER2:
		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_hrt_bw_set(
				mml_sysid_to_dpc_subsys(larb_idx),
				bw_in_mb, force_keep);
		/* report hrt read channel bw */
		if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
			mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist",
				__func__);
			return;
		}
		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_channel_bw_set_by_idx(
				mml_sysid_to_dpc_subsys(larb_idx),
				mml_sysid_to_dpc_hrt_read_idx(larb_idx),
				bw_in_mb);
		break;
	case DPC_VER3:
		mml_dpc_funcs.dpc_hrt_bw_set(
			mml_larb_idx_to_dpc_subsys(larb_idx),
			bw_in_mb, force_keep);
		/* report hrt read channel bw */
		if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
			mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist",
				__func__);
			return;
		}

		mml_dpc_funcs.dpc_channel_bw_set_by_idx(
			mml_larb_idx_to_dpc_subsys(larb_idx),
			mml_larb_idx_to_dpc_hrt_read_idx(larb_idx), bw_in_mb);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}
}

void mml_dpc_srt_bw_set(u32 larb_idx, const u32 bw_in_mb, bool force_keep)
{
	if (mml_dpc_funcs.dpc_srt_bw_set == NULL) {
		mml_msg_dpc("%s dpc_srt_bw_set not exist", __func__);
		return;
	}

	switch (mml_dpc_version) {
	case DPC_VER1:
		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_srt_bw_set(
				mml_sysid_to_dpc_subsys(larb_idx),
				bw_in_mb, force_keep);
		break;
	case DPC_VER2:
		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_srt_bw_set(
				mml_sysid_to_dpc_subsys(larb_idx),
				bw_in_mb, force_keep);
		/* report srt read channel bw */
		if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
			mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist",
				__func__);
			return;
		}

		/* larb_idx = sys_id */
		if (larb_idx == mml_sys_frame || larb_idx == mml_sys_tile)
			mml_dpc_funcs.dpc_channel_bw_set_by_idx(
				mml_sysid_to_dpc_subsys(larb_idx),
				mml_sysid_to_dpc_srt_read_idx(larb_idx),
				bw_in_mb);
		break;
	case DPC_VER3:
		mml_dpc_funcs.dpc_srt_bw_set(
			mml_larb_idx_to_dpc_subsys(larb_idx),
			bw_in_mb, force_keep);
		/* report srt read channel bw */
		if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
			mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist",
				__func__);
			return;
		}

		mml_dpc_funcs.dpc_channel_bw_set_by_idx(
			mml_larb_idx_to_dpc_subsys(larb_idx),
			mml_larb_idx_to_dpc_srt_read_idx(larb_idx), bw_in_mb);
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}
}

void mml_dpc_dvfs_set(const u8 level, bool force)
{
	if (mml_dpc_funcs.dpc_dvfs_set == NULL) {
		mml_msg_dpc("%s dpc_dvfs_set not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_set(DPC_SUBSYS_MML, level, force);
}

void mml_dpc_dvfs_trigger(void)
{
	if (mml_dpc_funcs.dpc_dvfs_trigger == NULL) {
		mml_msg_dpc("%s dpc_dvfs_trigger not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_dvfs_trigger("MML");
}

void mml_dpc_channel_bw_set_by_idx(u32 larb_idx, u32 bw, bool hrt)
{
	u8 idx = 0;

	if (mml_dpc_version == DPC_VER1)
		return;

	if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
		mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist", __func__);
		return;
	}

	switch (mml_dpc_version) {
	case DPC_VER2:
		/* larb_idx = sysid */
		switch (larb_idx) {
		case mml_sys_tile:
			idx = hrt ? 3 : 1;
			break;
		case mml_sys_frame:
			idx = hrt ? 11 : 9;
			break;
		default:
			return;
		}
		break;
	case DPC_VER3:
		switch (larb_idx) {
		case 0:
			idx = hrt ? 7 : 5;
			break;
		case 1:
			idx = hrt ? 15 : 13;
			break;
		case 2:
			idx = hrt ? 3 : 1;
			break;
		case 3:
			idx = hrt ? 11 : 9;
			break;
		default:
			idx = hrt ? 3 : 1;
			break;
		}
		break;
	default:
		mml_err("%s mml_dpc_version %d", __func__, mml_dpc_version);
		break;
	}

	mml_dpc_funcs.dpc_channel_bw_set_by_idx(DPC_SUBSYS_MML, idx, bw);
}

void mml_dpc_channel_bw_set(u32 sysid, u32 bw)
{
	u8 idx;

	if (mml_dpc_version == DPC_VER2 || mml_dpc_version == DPC_VER3)
		return;

	if (mml_dpc_funcs.dpc_channel_bw_set_by_idx == NULL) {
		mml_msg_dpc("%s dpc_channel_bw_set_by_idx not exist", __func__);
		return;
	}

	idx = 3;

	mml_dpc_funcs.dpc_channel_bw_set_by_idx(mml_sysid_to_dpc_subsys(sysid), idx, bw);
}

void mml_dpc_dump(void)
{
	if (!mml_dpc_funcs.dpc_analysis) {
		mml_msg_dpc("%s dpc_analysis not exist", __func__);
		return;
	}

	mml_dpc_funcs.dpc_analysis();
}
