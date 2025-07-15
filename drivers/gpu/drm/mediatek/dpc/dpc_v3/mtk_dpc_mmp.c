// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mtk_dpc_mmp.h"

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#else

static struct dpc_mmp_events_t dpc_mmp_events;

struct dpc_mmp_events_t *dpc_mmp_get_event(void)
{
	return &dpc_mmp_events;
}

void dpc_mmp_init(void)
{
	mmp_event folder;
	mmp_event hide;

	if (dpc_mmp_events.folder)
		return;

	mmprofile_enable(1);
	folder = mmprofile_register_event(MMP_ROOT_EVENT, "DPC");
	hide = mmprofile_register_event(folder, "hide");
	dpc_mmp_events.hide = hide;
	dpc_mmp_events.folder = folder;
	dpc_mmp_events.config = mmprofile_register_event(folder, "config");
	dpc_mmp_events.prete = mmprofile_register_event(folder, "prete");
	dpc_mmp_events.mminfra = mmprofile_register_event(folder, "mminfra");
	dpc_mmp_events.apsrc = mmprofile_register_event(folder, "apsrc");
	dpc_mmp_events.vlp_vote = mmprofile_register_event(folder, "vlp_vote");
	dpc_mmp_events.hwccf_vote = mmprofile_register_event(folder, "hwccf_vote");
	dpc_mmp_events.hwccf_gce_vote = mmprofile_register_event(hide, "hwccf_gce_vote");
	dpc_mmp_events.mml_sof = mmprofile_register_event(hide, "mml_sof");
	dpc_mmp_events.mml_rrot_done = mmprofile_register_event(hide, "mml_rrot_done");
	dpc_mmp_events.idle_off = mmprofile_register_event(hide, "idle_off");

	dpc_mmp_events.mtcmos_disp1_on = mmprofile_register_event(hide, "DISP1_ON");
	dpc_mmp_events.mtcmos_disp1_off = mmprofile_register_event(hide, "DISP1_OFF");
	dpc_mmp_events.mtcmos_mml2_on = mmprofile_register_event(folder, "MML2_ON");
	dpc_mmp_events.mtcmos_mml2_off = mmprofile_register_event(folder, "MML2_OFF");
	dpc_mmp_events.mtcmos_ovl0 = mmprofile_register_event(hide, "mtcmos_ovl0");
	dpc_mmp_events.mtcmos_disp1 = mmprofile_register_event(folder, "mtcmos_disp1");
	dpc_mmp_events.mtcmos_mml1 = mmprofile_register_event(folder, "mtcmos_mml1");
	dpc_mmp_events.vdisp_level = mmprofile_register_event(hide, "vdisp_level");
	dpc_mmp_events.mtcmos_auto = mmprofile_register_event(hide, "mtcmos_auto");
	dpc_mmp_events.ch_bw = mmprofile_register_event(hide, "ch_bw");
	dpc_mmp_events.hrt_bw = mmprofile_register_event(hide, "hrt_bw");
	dpc_mmp_events.srt_bw = mmprofile_register_event(hide, "srt_bw");
	dpc_mmp_events.debug1 = mmprofile_register_event(hide, "debug1");
	dpc_mmp_events.debug2 = mmprofile_register_event(hide, "debug2");

	dpc_mmp_events.user_9 = mmprofile_register_event(folder, "9_NST_LOCK");
	dpc_mmp_events.user_14 = mmprofile_register_event(folder, "14_FOR_FRAME");
	dpc_mmp_events.user_15 = mmprofile_register_event(folder, "15_DISP_ISR");
	dpc_mmp_events.user_16 = mmprofile_register_event(folder, "16_CRTC");
	dpc_mmp_events.user_17 = mmprofile_register_event(hide, "17_PQ");
	dpc_mmp_events.user_26 = mmprofile_register_event(hide, "26_DPC_CFG");
	dpc_mmp_events.user_31 = mmprofile_register_event(hide, "31_FORCE");
	dpc_mmp_events.user_19 = mmprofile_register_event(hide, "19_MML0");
	dpc_mmp_events.user_18 = mmprofile_register_event(hide, "18_MML1");
	dpc_mmp_events.user_12 = mmprofile_register_event(folder, "12_MML2");

	mmprofile_enable_event_recursive(folder, 1);
	mmprofile_start(1);
}

#endif
