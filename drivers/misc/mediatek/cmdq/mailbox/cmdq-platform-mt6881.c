// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <dt-bindings/gce/mt6881-gce.h>

#include "cmdq-util.h"
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
#include "proto.h"
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#include <mtk-smmu-v3.h>
#include <iommu_debug.h>
#endif

#define GCE_D_PA	0x1e980000
#define GCE_M_PA	0x1e990000
#define GCE_M_2_PA	0x1e9a0000

#define GCE_D_NORMAL_SID	199
#define GCE_M_NORMAL_SID	183


const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_D_PA || gce_pa == GCE_D_2_PA) {
		switch (thread) {
		case 0 ... 7:
		case 10 ... 12:
		case 22 ... 27:
		case 29 ... 31:
			return "MM_DISP";
		case 16 ... 19:
			return "MM_MML";
		case 20 ... 21:
			return "MM_MDP";
		default:
			return "MM_GCE";
		}
	} else if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 7:
		case 12 ... 13:
		case 16 ... 22:
		case 25 ... 29:
			return "MM_IMGSYS";
		case 8:
			return "MM_IMG_SLC";
		case 10:
			return "MM_IMG_DIP";
		case 11:
		case 23 ... 24:
			return "MM_IMG_FDVT";
		case 30:
			return "MM_MINI_MDP";
		default:
			return "MM_GCEM";
		}
	} else if (gce_pa == GCE_M_2_PA) {
		switch (thread) {
		case 0 ... 7:
		case 13:
		case 17:
			return "MM_IMGSYS";
		case 8:
			return "MM_IMG_SLC";
		case 10:
			return "MM_IMG_DIP";
		case 18 ... 23:
			return "MM_CAM_QOF";
		case 24 ... 28:
			return "MM_IMG_QOF";
		case 16:
			return "MM_CAM";
		case 29 ... 31:
			return "MM_I2C";
		default:
			return "MM_GCEM";
		}
	}

	return "MM_GCE";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	switch (event) {
	if (gce_pa == GCE_D_PA) // GCE-D
		switch (event) {
		case CMDQ_EVENT_MDPSYS_MDP_RDMA0_SOF
			... CMDQ_EVENT_MDPSYS_DPC_DISP1_MTCMOS_ON_PULSE:
			return "MM_MDP";
		case CMDQ_EVENT_DISPSYS_DISP_OVL0_2L_SOF
			... CMDQ_EVENT_DISPSYS_BUF_UNDERRUN_ENG_EVENT_7:
			return "MM_DISP";
		case CMDQ_EVENT_MML1_STREAM_SOF_0
			... CMDQ_EVENT_MML1_MDP_C3D0_FLIP_DONE_ENG_EVENT:
		case CMDQ_EVENT_MML2_STREAM_SOF_0
			... CMDQ_EVENT_MML2_MDP_C3D0_FLIP_DONE_ENG_EVENT:
		case CMDQ_SYNC_TOKEN_MML_BUFA
			... CMDQ_SYNC_TOKEN_MML_APU_START:
			return "MM_MML";
		default:
			return "MM_GCE";
		}

	if (gce_pa == GCE_M_PA) // GCE-M
		switch (event) {
		case CMDQ_EVENT_IMGSYS1_TRAW0_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMGSYS1_TRAW1_DMA_ERR_EVENT:
			return "MM_IMG_TRAW";
		case CMDQ_EVENT_IMGSYS1_DIP_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS1_DIP_NR_DMA_ERR_EVENT:
			return "MM_IMG_DIP";
		case CMDQ_EVENT_IMGSYS1_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMGSYS1_WPE_EIS_CQ_THR_DONE_P2_5:
		case CMDQ_EVENT_IMGSYS1_WPE_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMGSYS1_WPE_TNR_CQ_THR_DONE_P2_5:
			return "MM_IMG_WPE";
		case CMDQ_EVENT_IMGSYS1_PQDIP_A_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS1_PQA_DMA_ERR_EVENT:
		case CMDQ_EVENT_IMGSYS1_PQDIP_B_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS1_PQB_DMA_ERR_EVENT:
			return "MM_IMG_WPE";
		case CMDQ_EVENT_IMGSYS1_ME_DONE:
			return "MM_IMG_ME";
		case CMDQ_EVENT_IMGSYS1_IMGSYS_IPE_FDVT0_DONE:
			return "MM_IMG_FDVT";
		case CMDQ_EVENT_VENCSYS_VENC_FRAME_DONE
			... CMDQ_EVENT_VENCSYS_VENC_SLICE_DONE:
			return "MM_VENC";
		case CMDQ_EVENT_VDECSYS_VDEC_INT_LINE_CNT_GCE
			... CMDQ_EVENT_VDECSYS_VDEC_GCE_CNT_TG:
			return "MM_VDEC";
		/* sw event */
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_38:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_39:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_40
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_41:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_42
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_45:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_46
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_55:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_56
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_67:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_68
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_75:
		case CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_76
			... CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_100:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_1
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_133:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_134
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_151:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_152
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_205:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_206
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_221:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_222
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_250:
		case CMDQ_SYNC_TOKEN_IMGSYS_POOL_251
			... CMDQ_SYNC_TOKEN_IMGSYS_POOL_300:
		case CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS
			... CMDQ_SYNC_TOKEN_IPESYS_ME:
		case CMDQ_SYNC_TOKEN_IMGSYS_VSS_TRAW
			... CMDQ_SYNC_TOKEN_IMGSYS_VSS_DIP:
		case CMDQ_SYNC_TOKEN_IMGSYS_QOS_LOCK:
		case CMDQ_SYNC_TOKEN_IMGSYS_DFP
			... CMDQ_SYNC_TOKEN_IMGSYS_TRAW_SMT:
			return "MM_IMGSYS";
		case CMDQ_SYNC_TOKEN_IMGSYS_ADL:
			return "MM_CAM";
		case CMDQ_SYNC_TOKEN_IMGSYS_OMC_LITE:
			return "MM_OMC";
		case CMDQ_SYNC_TOKEN_DIP_POWER_CTRL
			... CMDQ_SYNC_TOKEN_TRAW_PWR_HAND_SHAKE:
			return "MM_IMG_DIP";
		case CMDQ_SYNC_TOKEN_DPE_POOL_1
			... CMDQ_SYNC_TOKEN_DPE_POOL_14:
			return "MM_IMG_FRM";
		/* sw event for sec*/
		case CMDQ_SYNC_TOKEN_TZMP_ISC_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_ISC_SET:
			return "MM_IMG_ISC";
		case CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_ISP_SET:
			return "MM_IMG_ISP";
		case CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_AIE_SET:
			return "MM_IMG_AIE";
		case CMDQ_SYNC_TOKEN_TZMP_ADL_WAIT
			... CMDQ_SYNC_TOKEN_TZMP_ADL_SET:
			return "MM_IMG_ADL";
		default:
			return "MM_GCEM";
		}

	if (gce_pa == GCE_M_2_PA) // GCE-M2
		switch (event) {
		case CMDQ_EVENT_IMGSYS2_TRAW0_CQ_THR_DONE_TRAW0_0
			... CMDQ_EVENT_IMGSYS2_TRAW1_DMA_ERR_EVENT:
			return "MM_IMG_TRAW";
		case CMDQ_EVENT_IMGSYS2_DIP_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS2_DIP_NR_DMA_ERR_EVENT:
			return "MM_IMG_DIP";
		case CMDQ_EVENT_IMGSYS2_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMGSYS2_WPE_EIS_CQ_THR_DONE_P2_5:
		case CMDQ_EVENT_IMGSYS2_WPE_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMGSYS2_WPE_TNR_CQ_THR_DONE_P2_5:
			return "MM_IMG_WPE";
		case CMDQ_EVENT_IMGSYS2_PQDIP_A_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS2_PQA_DMA_ERR_EVENT:
		case CMDQ_EVENT_IMGSYS2_PQDIP_B_CQ_THR_DONE_P2_0
			... CMDQ_EVENT_IMGSYS2_PQB_DMA_ERR_EVENT:
			return "MM_IMG_WPE";
		case CMDQ_EVENT_IMGSYS2_ME_DONE:
			return "MM_IMG_ME";
		case CMDQ_EVENT_IMGSYS2_IMGSYS_IPE_FDVT0_DONE:
			return "MM_IMG_FDVT";
		case CMDQ_EVENT_CAMSYS_CAM_EVENT_RESERVE_0
			... CMDQ_EVENT_CAMSYS_RAWA_CQ_DONE:
			return "MM_CAM";
		/* sw event */
		case CMDQ_SYNC_TOKEN_DIP_POWER_CTRL
			... CMDQ_SYNC_TOKEN_TRAW_PWR_HAND_SHAKE:
			return "MM_IMG_DIP";
		default:
			return "MM_GCEM";
		}

	return "MM_GCE";
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_D_PA:
		return 0;
	case GCE_M_PA:
		return 2;
	case GCE_M_2_PA:
		return 3;
	default:
		cmdq_err("unknown addr:%x", pa);
		return -1;
	}

	return 0;
}

u32 cmdq_test_get_subsys_list(u32 **regs_out)
{
	static u32 regs[] = {
		0x3e000100,	/* mmlsys0_config */
		0x32420000,	/* dispsys */
		0x34101200,	/* imgsys */
		0x1000106c,	/* infra */
	};

	*regs_out = regs;
	return ARRAY_SIZE(regs);
}

void cmdq_test_set_ostd(void)
{
	void __iomem	*va_base;
	u32 val = 0x01014000;
	u32 pa_base;
	u32 preval, newval;

	/* 1. set mdp_smi_common outstanding to 1 : 0x1E80F10C = 0x01014000 */
	pa_base = 0x1E80F10C;
	va_base = ioremap(pa_base, 0x1000);
	if (!va_base) {
		cmdq_err("va_base is NULL");
		return;
	}
	preval = readl(va_base);
	writel(val, va_base);
	newval = readl(va_base);
	cmdq_msg("%s addr0x%#x: 0x%#x -> 0x%#x  ", __func__, pa_base, preval, newval);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-D";

	if (hw_id == 1)
		return "GCE-M";

	if (hw_id == 2)
		return "GCE-M2";

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

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
s32 cmdq_get_thread_id(void *chan)
{
	const s32 idx = cmdq_mbox_chan_id(chan);

	return idx;
}

s32 cmdq_check_pkt_finalize(void *pkt)
{
	s32 result = cmdq_pkt_finalize((struct cmdq_pkt *)pkt);

	return result;
}
#endif

void cmdq_error_irq_debug(void *chan)
{
#ifndef CMDQ_SKIP_BY_CMDQ_BUILT
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#if !IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	struct device *dev = cmdq_mbox_get_dev(chan);
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));
	u32 sid;

	if (hw_id == 0 || hw_id == 1)
		sid = GCE_D_NORMAL_SID;
	else
		sid = GCE_M_NORMAL_SID;
	//dump smmu info to check gce va mode
	mtk_smmu_reg_dump(MM_SMMU, dev, sid);
	mtk_smmu_wpreg_dump(NULL, MM_SMMU);
	mtk_smmu_pgtable_dump(NULL, MM_SMMU, true);
	//dump gce req
	cmdq_mbox_dump_gce_req(chan);
#endif
#endif
#endif
}

bool cmdq_check_tf(struct device *dev,
	u32 sid, u32 tbu, u32 *axids)
{
#ifndef CMDQ_SKIP_BY_CMDQ_BUILT
#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
#if !IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	struct mtk_smmu_fault_param out_param;

	return mtk_smmu_tf_detect(MM_SMMU, dev,
		sid, tbu, axids, 1, &out_param);
#else
	return false;
#endif
#else
	return false;
#endif
#else
	return false;
#endif
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.test_set_ostd = cmdq_test_set_ostd,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
	.hw_trace_thread = cmdq_mbox_hw_trace_thread,
	.dump_error_irq_debug = cmdq_error_irq_debug,
	.check_tf = cmdq_check_tf,
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	.get_thread_id = cmdq_get_thread_id,
	.check_pkt_finalize = cmdq_check_pkt_finalize,
#endif
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ)
	virtio_cmdq_util_set_fp(&platform_fp);
#endif
	return 0;
}
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL");
