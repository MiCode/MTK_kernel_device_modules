// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <dt-bindings/memory/mt6761-larb-port.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include "mmqos-mtk.h"

static const struct mtk_node_desc node_descs_mt6761[] = {
	DEFINE_MNODE(common0,
		SLAVE_COMMON(0), 0, false, 0x0, MMQOS_NO_LINK),
	DEFINE_MNODE(common0_port0,
		MASTER_COMMON_PORT(0, 0), 0, false, 0x1, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port1,
		MASTER_COMMON_PORT(0, 1), 0, false, 0x2, SLAVE_COMMON(0)),
	DEFINE_MNODE(common0_port2,
		MASTER_COMMON_PORT(0, 2), 0, false, 0x2, SLAVE_COMMON(0)),
    /*SMI Common*/
	DEFINE_MNODE(larb0, SLAVE_LARB(0), 0, false, 0x0, MASTER_COMMON_PORT(0, 0)),
	DEFINE_MNODE(larb1, SLAVE_LARB(1), 0, false, 0x0, MASTER_COMMON_PORT(0, 1)),
	DEFINE_MNODE(larb2, SLAVE_LARB(2), 0, false, 0x0, MASTER_COMMON_PORT(0, 2)),
    /*Larb 0*/
	DEFINE_MNODE(l0_disp_ovl0,
		MASTER_LARB_PORT(M4U_PORT_DISP_OVL0), 8, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_ovl0_2l,
		MASTER_LARB_PORT(M4U_PORT_DISP_2L_OVL0_LARB0), 8, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_rdma0,
		MASTER_LARB_PORT(M4U_PORT_DISP_RDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_w_disp_wdma0,
		MASTER_LARB_PORT(M4U_PORT_DISP_WDMA0), 9, true, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_rdma0,
		MASTER_LARB_PORT(M4U_PORT_MDP_RDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_wdma0,
		MASTER_LARB_PORT(M4U_PORT_MDP_WDMA0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_mdp_wrot0,
		MASTER_LARB_PORT(M4U_PORT_MDP_WROT0), 7, false, 0x0, SLAVE_LARB(0)),
	DEFINE_MNODE(l0_disp_fake,
		MASTER_LARB_PORT(M4U_PORT_DISP_FAKE0), 7, false, 0x0, SLAVE_LARB(0)),
    /*Larb 1*/
	DEFINE_MNODE(l1_venc_rcpu,
		MASTER_LARB_PORT(M4U_PORT_VENC_RCPU), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_rec,
		MASTER_LARB_PORT(M4U_PORT_VENC_REC), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_bsdma,
		MASTER_LARB_PORT(M4U_PORT_VENC_BSDMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_sv_comv,
		MASTER_LARB_PORT(M4U_PORT_VENC_SV_COMV), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_rd_comv,
		MASTER_LARB_PORT(M4U_PORT_VENC_RD_COMV), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_jpgenc_rdma,
		MASTER_LARB_PORT(M4U_PORT_JPGENC_RDMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_jpgenc_bsdma,
		MASTER_LARB_PORT(M4U_PORT_JPGENC_BSDMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_cur_luma,
		MASTER_LARB_PORT(M4U_PORT_VENC_CUR_LUMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_cur_chroma,
		MASTER_LARB_PORT(M4U_PORT_VENC_CUR_CHROMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_ref_luma,
		MASTER_LARB_PORT(M4U_PORT_VENC_REF_LUMA), 8, false, 0x0, SLAVE_LARB(1)),
	DEFINE_MNODE(l1_venc_ref_chroma,
		MASTER_LARB_PORT(M4U_PORT_VENC_REF_CHROMA), 8, false, 0x0, SLAVE_LARB(1)),
    /*Larb 2*/
	DEFINE_MNODE(l2_imgo,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rrzo,
		MASTER_LARB_PORT(M4U_PORT_CAM_RRZO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_aao,
		MASTER_LARB_PORT(M4U_PORT_CAM_AAO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_lcso,
		MASTER_LARB_PORT(M4U_PORT_CAM_LCSO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_esfko,
		MASTER_LARB_PORT(M4U_PORT_CAM_ESFKO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_imgo_s,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGO_S), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_imgo_s2,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGO_S2), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_lsci,
		MASTER_LARB_PORT(M4U_PORT_CAM_LSCI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_lsci_d,
		MASTER_LARB_PORT(M4U_PORT_CAM_LSCI_D), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_afo,
		MASTER_LARB_PORT(M4U_PORT_CAM_AFO), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_afo_d,
		MASTER_LARB_PORT(M4U_PORT_CAM_AFO_D), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_bpci,
		MASTER_LARB_PORT(M4U_PORT_CAM_BPCI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_bpci_d,
		MASTER_LARB_PORT(M4U_PORT_CAM_BPCI_D), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_ufdi,
		MASTER_LARB_PORT(M4U_PORT_CAM_UFDI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_imgi,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMGI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_img2o,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMG2O), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_img3o,
		MASTER_LARB_PORT(M4U_PORT_CAM_IMG3O), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_vipi,
		MASTER_LARB_PORT(M4U_PORT_CAM_VIPI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_vip2i,
		MASTER_LARB_PORT(M4U_PORT_CAM_VIP2I), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_vip3i,
		MASTER_LARB_PORT(M4U_PORT_CAM_VIP3I), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_icei,
		MASTER_LARB_PORT(M4U_PORT_CAM_ICEI), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rp,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_RP), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_wr,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_WR), 8, false, 0x0, SLAVE_LARB(2)),
	DEFINE_MNODE(l2_rb,
		MASTER_LARB_PORT(M4U_PORT_CAM_FD_RB), 8, false, 0x0, SLAVE_LARB(2)),
};
static const char * const comm_muxes_mt6761[] = { "mm" };
static const char * const comm_icc_path_names_mt6761[] = { "icc-bw" };
static const char * const comm_icc_hrt_path_names_mt6761[] = { "icc-hrt-bw" };
static const struct mtk_mmqos_desc mmqos_desc_mt6761 = {
	.nodes = node_descs_mt6761,
	.num_nodes = ARRAY_SIZE(node_descs_mt6761),
	.comm_muxes = comm_muxes_mt6761,
	.comm_icc_path_names = comm_icc_path_names_mt6761,
	.comm_icc_hrt_path_names = comm_icc_hrt_path_names_mt6761,
	.max_ratio = 64,
	.hrt = {
		.hrt_bw = {5332, 0, 0},
		.hrt_total_bw = 22000, /*Todo: Use DRAMC API 5500*2(channel)*2(io width)*/
		.md_speech_bw = { 5332, 5332},
		.hrt_ratio = {1000, 860, 880, 1000}, /* MD, CAM, DISP, MML */
		.blocking = true,
		.emi_ratio = 705,
	},
	.hrt_LPDDR4 = {
		.hrt_bw = {5141, 0, 0},
		.hrt_total_bw = 17064, /*Todo: Use DRAMC API 4266*2(channel)*2(io width)*/
		.md_speech_bw = { 5141, 5141},
		.hrt_ratio = {1000, 880, 900, 1000}, /* MD, CAM, DISP, MML */
		.blocking = true,
		.emi_ratio = 800,
	},
	.comm_port_channels = {
		{ 0x1, 0x2, 0x2}
	},
	.comm_port_hrt_types = {
		{ HRT_MAX_BWL, HRT_NONE, HRT_NONE, HRT_NONE, HRT_NONE, HRT_DISP },
	},
};
static const struct of_device_id mtk_mmqos_mt6761_of_ids[] = {
	{
		.compatible = "mediatek,mt6761-mmqos",
		.data = &mmqos_desc_mt6761,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_mmqos_mt6761_of_ids);
static struct platform_driver mtk_mmqos_mt6761_driver = {
	.probe = mtk_mmqos_probe,
	.remove = mtk_mmqos_remove,
	.driver = {
		.name = "mtk-mt6761-mmqos",
		.of_match_table = mtk_mmqos_mt6761_of_ids,
	},
};
module_platform_driver(mtk_mmqos_mt6761_driver);
MODULE_LICENSE("GPL");

