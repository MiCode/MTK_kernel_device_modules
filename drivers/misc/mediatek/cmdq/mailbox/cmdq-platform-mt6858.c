// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6858-gce.h>

#include "cmdq-util.h"

#define GCE_D_PA	0x1e980000
#define GCE_M_PA	0x1e990000

#define MDP_THRD_MIN	20

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
		case 22:
		case 24 ... 25:
			return "MM_DISP";
		case 16 ... 19:
			return "MM_MDP";
		default:
			return "MM_GCE";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 5:
		case 10 ... 11:
		case 16 ... 24:
			return "MM_ISP";
		case 6 ... 7:
			return "MM_VFMT";
		case 12:
			return "MM_VENC";
		default:
			return "MM_GCE";
		}
	}

	return "CMDQ";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	case CMDQ_EVENT_GPR_TIMER ... CMDQ_EVENT_GPR_TIMER + 32:
		return cmdq_thread_module_dispatch(gce_pa, thread);
	}

	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		/*HW Event*/
		case CMDQ_EVENT_MDPSYS_MDP_RDMA0_SOF
			... CMDQ_EVENT_MDPSYS_DPC_DISP1_MTCMOS_ON_PULSE:
			return "MM_MDP";
		case CMDQ_EVENT_DISPSYS_DISP_OVL0_2L_SOF
			... CMDQ_EVENT_DISPSYS_BUF_UNDERRUN_ENG_EVENT_BIT7:
			return "MM_DISP";
		/*SW Event*/
		case CMDQ_SYNC_TOKEN_HISTOGRAM_MDP_WAIT
			... CMDQ_SYNC_TOKEN_HISTOGRAM_MDP_SET:
			return "MM_MDP";
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
		case CMDQ_SYNC_TOKEN_STREAM_EOF:
		case CMDQ_SYNC_TOKEN_ESD_EOF:
		case CMDQ_SYNC_TOKEN_STREAM_BLOCK:
		case CMDQ_SYNC_TOKEN_CABC_EOF:
		case CMDQ_SYNC_TOKEN_VFP_PERIOD:
		case CMDQ_SYNC_TOKEN_VDO_MODE_CABC_EOF:
			return "MM_DISP";
		case CMDQ_SYNC_TOKEN_USER_0
			... CMDQ_SYNC_TOKEN_USER_1:
			return "MM_MDP";
		case CMDQ_SYNC_TOKEN_TZMP_DISP_WAIT:
		case CMDQ_SYNC_TOKEN_TZMP_DISP_SET:
			return "MM_DISP";
		case CMDQ_SYNC_TOKEN_MML_BUFA
			... CMDQ_SYNC_TOKEN_MML_APU_START:
		case CMDQ_SYNC_TOKEN_PREBUILT_MDP_LOCK:
			return "MM_MDP";
		case CMDQ_SYNC_TOKEN_PREBUILT_MML_LOCK:
			return "MM_MML";
		case CMDQ_SYNC_TOKEN_PREBUILT_DISP_LOCK:
		case CMDQ_SYNC_TOKEN_DISP_VA_START
			... CMDQ_SYNC_TOKEN_DISP_VA_END:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_1:
		case CMDQ_SYNC_TOKEN_STREAM_EOF_1:
		case CMDQ_SYNC_TOKEN_ESD_EOF_1:
		case CMDQ_SYNC_TOKEN_STREAM_BLOCK_1:
		case CMDQ_SYNC_TOKEN_CABC_EOF_1:
			return "MM_DISP";
		case CMDQ_SYNC_TOKEN_GPR_SET_0
			... CMDQ_SYNC_TOKEN_GPR_SET_4:
			return "MM_MDP";
		case CMDQ_SYNC_TOKEN_TE_0:
		case CMDQ_SYNC_TOKEN_PREFETCH_TE_0:
		case CMDQ_SYNC_TOKEN_VIDLE_POWER_ON:
		case CMDQ_SYNC_TOKEN_CHECK_TRIGGER_MERGE:
		case CMDQ_SYNC_RESOURCE_WROT0:
		case CMDQ_SYNC_RESOURCE_WROT1:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_3:
		case CMDQ_SYNC_TOKEN_STREAM_EOF_3:
		case CMDQ_SYNC_TOKEN_ESD_EOF_3:
		case CMDQ_SYNC_TOKEN_STREAM_BLOCK_3:
		case CMDQ_SYNC_TOKEN_CABC_EOF_3:
			return "MM_DISP";
		default:
			return "MM_GCE";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		/*HW Event*/
		case CMDQ_EVENT_VENC_VENC_CMDQ_FRAME_DONE
			... CMDQ_EVENT_VENC_VENC_CMDQ_VPS_DONE:
			return "MM_VENC";
		case CMDQ_EVENT_VDEC_GCE_EVENT_0
			... CMDQ_EVENT_VDEC_GCE_EVENT_15:
			return "MM_VENC";
		case CMDQ_EVENT_CAM_ISP_FRAME_DONE_A
			... CMDQ_EVENT_CAM_SENINF_CAM14_FIFO_FULL:
			return "MM_CAM";
		case CMDQ_EVENT_IMG1_DIP_FRAME_DONE_P2_0
			... CMDQ_EVENT_IMG1_DIP_DMA_ERR_EVENT:
		case CMDQ_EVENT_IMG2_DIP_FRAME_DONE_P2_0
			... CMDQ_EVENT_IMG2_DIP_DMA_ERR_EVENT:
			return "MM_IMG_DIP";
		case CMDQ_EVENT_IMG1_AMD_FRAME_DONE:
		case CMDQ_EVENT_IMG2_AMD_FRAME_DONE:
			return "MM_IMG_AMD";
		case CMDQ_EVENT_IMG1_MFB_DONE_LINK_MISC:
		case CMDQ_EVENT_IMG2_MFB_DONE_LINK_MISC:
			return "MM_IMG_MFB";
		case CMDQ_EVENT_IMG1_WPE_A_DONE_LINK_MISC:
		case CMDQ_EVENT_IMG2_WPE_A_DONE_LINK_MISC:
			return "MM_IMG_WPE";
		case CMDQ_EVENT_IMG1_MSS_DONE_LINK_MISC:
		case CMDQ_EVENT_IMG2_MSS_DONE_LINK_MISC:
			return "MM_IMG_MSS";
		case CMDQ_EVENT_IPE_FDVT_DONE
			... CMDQ_EVENT_IPE_DVP_DONE_ASYNC_SHOT:
			return "MM_IPE";
		/*SW Event*/
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_100:
			return "MM_IMG";
		case CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_ISP_SET:
			return "MM_ISP";
		case CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_AIE_SET:
			return "MM_AIE";
		case CMDQ_SYNC_TOKEN_TZMP_ADL_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_ADL_SET:
			return "MM_ADL";
		case CMDQ_SYNC_TOKEN_MSS:
		case CMDQ_SYNC_TOKEN_MSF:
			return "MM_IMG";
		case CMDQ_SYNC_TOKEN_PREBUILT_VFMT_LOCK:
			return "VFMT";
		default:
			return "MM_GCEM";
		}
	return "CMDQ";
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_D_PA:
		return 0;
	case GCE_M_PA:
		return 1;
	default:
		cmdq_err("unknown addr:%x", pa);
	}

	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x1f003000,	/* mdp_wrot0 */
		0x14000100,	/* mmsys_config */
		0x14001000,	/* dispsys */
		0x15101200,	/* imgsys */
		0x1000106c,	/* infra */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-D";

	if (hw_id == 1)
		return "GCE-M";

	return "CMDQ";
}

bool cmdq_thread_ddr_module(const s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
	case 15:
		return false;
	default:
		return true;
	}
}

bool cmdq_mbox_hw_trace_thread(void *chan)
{
	const phys_addr_t gce_pa = cmdq_mbox_get_base_pa(chan);
	const s32 idx = cmdq_mbox_chan_id(chan);

	if (gce_pa == GCE_D_PA)
		switch (idx) {
		case 16 ... 19: // MML
			cmdq_log("%s: pa:%pa idx:%d", __func__, &gce_pa, idx);
			return false;
		}

	return true;
}

void cmdq_error_irq_debug(void *chan)
{
}

bool cmdq_check_tf(struct device *dev,
	u32 sid, u32 tbu, u32 *axids)
{
	return false;
}

uint cmdq_get_mdp_min_thread(void)
{
	return MDP_THRD_MIN;
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
	.hw_trace_thread = cmdq_mbox_hw_trace_thread,
	.dump_error_irq_debug = cmdq_error_irq_debug,
	.check_tf = cmdq_check_tf,
	.get_mdp_min_thread = cmdq_get_mdp_min_thread,
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}

module_init(cmdq_platform_init);

MODULE_LICENSE("GPL");
