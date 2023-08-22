// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6897-gce.h>

#include "cmdq-util.h"

#define GCE_D_PA	0x1e980000
#define GCE_M_PA	0x1e990000

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 9:
		case 22:
		case 24 ... 25:
			return "DISP";
		case 16 ... 19:
			return "MML";
		case 20 ... 21:
			return "MDP";
		default:
			return "CMDQ";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 5:
		case 10 ... 11:
		case 16 ... 24:
			return "ISP";
		case 6 ... 7:
			return "VFMT";
		case 12:
			return "VENC";
		default:
			return "CMDQ";
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
		case CMDQ_EVENT_OVLSYS_DISP_OVL0_2L_SOF
			... CMDQ_EVENT_OVLSYS1_BUF_UNDERRUN_ENG_EVENT_7:
			return "DISP";
		case CMDQ_EVENT_MMLSYS1_MDP_RDMA0_SOF
			... CMDQ_EVENT_MMLSYS1_BUF_UNDERRUN_ENG_EVENT_2:
			return "MML";
		case CMDQ_EVENT_MMLSYS_MDP_RDMA0_SOF
			... CMDQ_EVENT_MMLSYS_BUF_UNDERRUN_ENG_EVENT_2:
			return "MDP";
		case CMDQ_EVENT_DISPSYS_DISP_AAL0_SOF
			... CMDQ_EVENT_DISPSYS1_BUF_UNDERRUN_7:
			return "DISP";
		case CMDQ_EVENT_GCE_EVENT_DSI0_TE_I
			... CMDQ_EVENT_DPTX_DPTX_EVENT_1:
			return "DISP";
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
		case CMDQ_SYNC_TOKEN_STREAM_EOF:
		case CMDQ_SYNC_TOKEN_ESD_EOF:
		case CMDQ_SYNC_TOKEN_STREAM_BLOCK:
		case CMDQ_SYNC_TOKEN_CABC_EOF:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_1
			... CMDQ_SYNC_TOKEN_CABC_EOF_1:
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY_3
			... CMDQ_SYNC_TOKEN_CABC_EOF_3:
			return "DISP";
		case CMDQ_SYNC_TOKEN_MML_BUFA
			... CMDQ_SYNC_TOKEN_MML_PIPE1_NEXT:
			return "MML";
		default:
			return "CMDQ";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_VENC2_VENC_FRAME_DONE
			... CMDQ_EVENT_VENC1_VENC_SOC_FRAME_DONE:
			return "VENC";
		case CMDQ_EVENT_IMG_IMG_SOF:
			return "IMG";
		case CMDQ_EVENT_IMG_TRAW0_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW0_DIP_DMA_ERR_EVENT:
			return "TRAW";
		case CMDQ_EVENT_IMG_TRAW1_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMG_TRAW1_DIP_DMA_ERR_EVENT:
			return "LTRAW";
		case CMDQ_EVENT_IMG_ADL_TILE_DONE_EVENT:
			return "ADL";
		case CMDQ_EVENT_IMG_DIP_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_DIP_DUMMY_2:
			return "DIP";
		case CMDQ_EVENT_IMG_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WOE_EIS_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE0_DUMMY_0
			... CMDQ_EVENT_IMG_WPE0_DUMMY_2:
			return "WPE_EIS";
		case CMDQ_EVENT_IMG_PQDIP_A_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQA_DMA_ERR_EVENT:
			return "PQDIP_A";
		case CMDQ_EVENT_IMG_WPE_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WOE_TNR_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE1_DUMMY_0
			... CMDQ_EVENT_IMG_WPE1_DUMMY_2:
			return "WPE_TNR";
		case CMDQ_EVENT_IMG_PQDIP_B_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMG_PQB_DMA_ERR_EVENT:
			return "PQDIP_B";
		case CMDQ_EVENT_IMG_WPE_LITE_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WOE_LITE_CQ_THR_DONE_P2_9:
		case CMDQ_EVENT_IMG_WPE2_DUMMY_0
			... CMDQ_EVENT_IMG_WPE2_DUMMY_2:
			return "WPE_LITE";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_FDVT0_DONE:
			return "FDVT";
		case CMDQ_EVENT_IMG_IMGSYS_IPE_ME_DONE:
		case CMDQ_EVENT_IMG_IMGSYS_IPE_MMG_DONE:
			return "ME";
		case CMDQ_EVENT_CAM_CAM_SUBA_SW_PASS1_DONE
			... CMDQ_EVENT_CAM_PDA1_IRQO_EVENT_DONE_D1:
		case CMDQ_EVENT_CAM_CAM_SUBA_TG_INT1
			... CMDQ_EVENT_CAM_DVGF_CMDQ_EVENT:
			return "CAM";
		case CMDQ_EVENT_CAM_DPE_DVP_CMQ_EVENT:
		case CMDQ_EVENT_CAM_DPE_DVS_CMQ_EVENT:
			return "DPE";
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_133:
		case CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS
			... CMDQ_SYNC_TOKEN_IPESYS_ME:
		case CMDQ_SYNC_TOKEN_IMGSYS_VSS_TRAW
			... CMDQ_SYNC_TOKEN_IMGSYS_VSS_DIP:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_134
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_221:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_222
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_250:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_251
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_300:
			return "IMGSYS";
		case CMDQ_SYNC_TOKEN_APUSYS_APU:
			return "APUSYS_EDMA";
		default:
			return "CMDQ";
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
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}

module_init(cmdq_platform_init);

MODULE_LICENSE("GPL v2");
