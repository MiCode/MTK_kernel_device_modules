/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_MMP_H__
#define __MTK_MML_MMP_H__

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
#ifndef MML_FPGA
#if IS_ENABLED(CONFIG_MMPROFILE)
#define MML_MMP_SUPPORT
#endif
#endif
#endif


#ifdef MML_MMP_SUPPORT
#include <mmprofile.h>
#include <mmprofile_function.h>

#define mmp_data2_fence(c, s)	((c & 0xff) << 24 | s & 0xffffff)

#define mml_mmp(event, flag, v1, v2) \
	mmprofile_log_ex(mml_mmp_get_event()->event, flag, v1, v2)

#define mml_mmp2(event, flag, v1h, v1l, v2h, v2l) \
	mmprofile_log_ex(mml_mmp_get_event()->event, flag, v1h << 16 | v1l, v2h << 16 | v2l)

#define mml_mmp_raw(event, flag, v1, v2, data_ptr, sz) do { \
	struct mmp_metadata_t meta = { \
		.data1 = v1, \
		.data2 = v2, \
		.data_type = MMPROFILE_META_RAW, \
		.size = sz, \
		.p_data = data_ptr, \
	}; \
	\
	mmprofile_log_meta(mml_mmp_get_event()->event, flag, &meta); \
} while(0)

struct mml_mmp_events_t {
	mmp_event mml;
	mmp_event query_mode;
	mmp_event submit;
	mmp_event submit_info;
	mmp_event config;
	mmp_event config_dle;
	mmp_event dumpinfo;
	mmp_event task_create;
	mmp_event buf_map;
	mmp_event comp_prepare;
	mmp_event buf_prepare;
	mmp_event tile_alloc;
	mmp_event tile_calc;
	mmp_event tile_calc_frame;
	mmp_event tile_prepare_tile;
	mmp_event command;
	mmp_event fence;
	mmp_event fence_timeout;
	mmp_event wait_ready;
	mmp_event throughput;
	mmp_event bandwidth;
	mmp_event flush;
	mmp_event submit_cb;
	mmp_event racing_enter;
	mmp_event racing_stop;
	mmp_event racing_stop_sync;
	mmp_event irq_loop;
	mmp_event irq_err;
	mmp_event irq_done;
	mmp_event irq_stop;
	mmp_event fence_sig;
	mmp_event exec;

	/* events for command (dle and pipes) */
	mmp_event command0;
	mmp_event command1;
	mmp_event mutex_mod;
	mmp_event mutex_en;

	/* events for clock */
	mmp_event clock;
	mmp_event clk_enable;
	mmp_event clk_disable;

	/* events for inline rotate disp addon */
	mmp_event addon;
	mmp_event addon_mml_calc_cfg;
	mmp_event addon_addon_config;
	mmp_event addon_start;
	mmp_event addon_unprepare;
	mmp_event addon_dle_config;

	/* events for dle adaptor */
	mmp_event dle;
	mmp_event dle_config_create;
	mmp_event dle_buf;

	/*events for dle PQ irq*/
	mmp_event dle_aal_irq_done;

	mmp_event dpc;
	mmp_event dpc_cfg;
	mmp_event dpc_exception_flow;
	mmp_event dpc_pm_runtime_get;
	mmp_event dpc_pm_runtime_put;
};

void mml_mmp_init(void);
struct mml_mmp_events_t *mml_mmp_get_event(void);
#else

#define mml_mmp(args...)
#define mml_mmp2(args...)
#define mml_mmp_raw(args...)

static inline void mml_mmp_init(void)
{
}
#endif

#endif	/* __MTK_MML_MMP_H__ */
