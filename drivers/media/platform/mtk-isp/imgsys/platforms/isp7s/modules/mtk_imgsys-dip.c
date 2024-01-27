// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-dip.h"

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary[] = {
	{0x0A8, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
};

static struct DIPDmaDebugInfo g_DMATopDbgIfo[] = {
	{"IMGI", DIP_ORI_RDMA_DEBUG},
	{"IMGI_UFD", DIP_ORI_RDMA_UFD_DEBUG},
	{"IMGBI", DIP_ORI_RDMA_DEBUG},
	{"IMGBI_UFD", DIP_ORI_RDMA_UFD_DEBUG},
	{"IMGCI", DIP_ORI_RDMA_DEBUG},
	{"IMGDI", DIP_ORI_RDMA_UFD_DEBUG},
	{"DEPI", DIP_ORI_RDMA_DEBUG},
	{"DEPI_UFD", DIP_ORI_RDMA_UFD_DEBUG},
	{"DMGI", DIP_ORI_RDMA_DEBUG},
	{"TNRWI", DIP_ULC_RDMA_DEBUG},
	{"TNRMI", DIP_ULC_RDMA_DEBUG},
	{"TNRCI", DIP_ULC_RDMA_DEBUG},
	{"TNRVBI", DIP_ULC_RDMA_DEBUG},
	{"TNRLYI", DIP_ULC_RDMA_DEBUG},
	{"TNRLCI", DIP_ULC_RDMA_DEBUG},
	{"TNRSI", DIP_ULC_RDMA_DEBUG},
	{"TNRAIMI", DIP_ULC_RDMA_DEBUG},
	{"RECI_D1", DIP_ULC_RDMA_DEBUG},
	{"RECBI_D1", DIP_ULC_RDMA_DEBUG},
	{"RECI_D2", DIP_ULC_RDMA_DEBUG},
	{"RECBI_D2", DIP_ULC_RDMA_DEBUG},
	{"RECBI_D3", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D1", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D2", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D3", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D8", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D1", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D2", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D8", DIP_ULC_RDMA_DEBUG},
	{"IMG4O", DIP_ORI_WDMA_DEBUG},
	{"IMG4BO", DIP_ORI_WDMA_DEBUG},
	{"IMG4CO", DIP_ORI_WDMA_DEBUG},
	{"IMG4DO", DIP_ORI_WDMA_DEBUG},
	{"TNRWO", DIP_ULC_WDMA_DEBUG},
	{"TNRMO", DIP_ULC_WDMA_DEBUG},
	{"TNRSO", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D1", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D2", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D3", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D8", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D1", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D2", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D8", DIP_ULC_WDMA_DEBUG},
};

static struct DIPDmaDebugInfo g_DMANrDbgIfo[] = {
	{"VIPI", DIP_ULC_RDMA_DEBUG},
	{"VIPBI", DIP_ULC_RDMA_DEBUG},
	{"VIPCI", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D4", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D5", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D6", DIP_ULC_RDMA_DEBUG},
	{"SMTI_D9", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D4", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D5", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D6", DIP_ULC_RDMA_DEBUG},
	{"SMTCI_D9", DIP_ULC_RDMA_DEBUG},
	{"EECSI", DIP_ULC_RDMA_DEBUG},
	{"SNRSI", DIP_ULC_RDMA_DEBUG},
	{"SNRCSI", DIP_ULC_RDMA_DEBUG},
	{"SNRAIMI", DIP_ULC_RDMA_DEBUG},
	{"CSMCI", DIP_ULC_RDMA_DEBUG},
	{"CSMCSI", DIP_ULC_RDMA_DEBUG},
	{"CSMCSTI", DIP_ULC_RDMA_DEBUG},
	{"IMG3O", DIP_ORI_WDMA_DEBUG},
	{"IMG3BO", DIP_ORI_WDMA_DEBUG},
	{"IMG3CO", DIP_ORI_WDMA_DEBUG},
	{"IMG3DO", DIP_ORI_WDMA_DEBUG},
	{"FEO", DIP_ULC_WDMA_DEBUG},
	{"IMG2O", DIP_ULC_WDMA_DEBUG},
	{"IMG2BO", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D4", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D5", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D6", DIP_ULC_WDMA_DEBUG},
	{"SMTO_D9", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D4", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D5", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D6", DIP_ULC_WDMA_DEBUG},
	{"SMTCO_D9", DIP_ULC_WDMA_DEBUG},
};

#define DIP_HW_SET 3

#define	DIP_INIT_ARRAY_COUNT	1

static void __iomem *gdipRegBA[DIP_HW_SET] = {0L};
static unsigned int g_RegBaseAddr = DIP_TOP_ADDR;

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;


	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_NR2; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdipRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdipRegBA[ary_idx]) {
			pr_info("%s:unable to iomap dip_%d reg, devnode(%s)\n",
				__func__, hw_idx);
			continue;
		}
	}


}

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i;


	/* iomap registers */
	dipRegBA = gdipRegBA[0];

	for (i = 0 ; i < DIP_INIT_ARRAY_COUNT; i++) {
		ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
		writel(mtk_imgsys_dip_init_ary[i].val, ofset);
	}

}

static unsigned int ExeDbgCmd(struct mtk_imgsys_dev *a_pDev,
			void __iomem *a_pRegBA,
			unsigned int a_DdbSel,
			unsigned int a_DbgOut,
			unsigned int a_DbgCmd)
{
	unsigned int DbgData = 0;
	unsigned int DbgOutReg = g_RegBaseAddr + a_DbgOut;
	void __iomem *pDbgSel = (void *)(a_pRegBA + a_DdbSel);
	void __iomem *pDbgPort = (void *)(a_pRegBA + a_DbgOut);

	iowrite32(a_DbgCmd, pDbgSel);
	DbgData = (unsigned int)ioread32(pDbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		a_DbgCmd, DbgOutReg, DbgData);

	return DbgData;
}

static void imgsys_dip_dump_dma(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut,
				char a_DMANrPort)
{
	unsigned int Idx = 0;
	unsigned int DbgCmd = 0;
	unsigned int DmaDegInfoSize = sizeof(struct DIPDmaDebugInfo);
	unsigned int DebugCnt = sizeof(g_DMATopDbgIfo)/DmaDegInfoSize;
	enum DIPDmaDebugType DbgTy = DIP_ORI_RDMA_DEBUG;

	/* DMA NR */
	if (a_DMANrPort == 1)
		DebugCnt = sizeof(g_DMANrDbgIfo)/DmaDegInfoSize;

	/* Dump DMA Debug Info */
	for (Idx = 0; Idx < DebugCnt; Idx++) {
		/* state_checksum */
		DbgCmd = DIP_IMGI_STATE_CHECKSUM + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt_tmp */
		DbgCmd = DIP_IMGI_LINE_PIX_CNT_TMP + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt */
		DbgCmd = DIP_IMGI_LINE_PIX_CNT + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

		if (a_DMANrPort == 1)
			DbgTy = g_DMANrDbgIfo[Idx].DMADebugType;
		else
			DbgTy = g_DMATopDbgIfo[Idx].DMADebugType;

		/* important_status */
		if (DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_IMPORTANT_STATUS + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* smi_debug_data (case 0) or cmd_data_cnt */
		if (DbgTy == DIP_ORI_RDMA_DEBUG ||
			DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_SMI_DEBUG_DATA_CASE0 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* ULC_RDMA or ULC_WDMA */
		if (DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_TILEX_BYTE_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_TILEY_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* smi_dbg_data(case 0) or burst_line_cnt or input_v_cnt */
		if (DbgTy == DIP_ORI_WDMA_DEBUG ||
			DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_BURST_LINE_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_RDMA */
		if (DbgTy == DIP_ORI_RDMA_DEBUG) {
			DbgCmd = DIP_IMGI_FIFO_DEBUG_DATA_CASE1 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_FIFO_DEBUG_DATA_CASE3 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_WDMA */
		if (DbgTy == DIP_ORI_WDMA_DEBUG) {
			DbgCmd = DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE1 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE3 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* xfer_y_cnt */
		if (DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_XFER_Y_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}
	}
}


static void imgsys_dip_dump_dl(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int DbgLineCnt = 0, DbgRdy = 0, DbgReq = 0;
	unsigned int DbgLineCntReg = 0;

	/* wpe_wif_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000006;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000007;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000008;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* wpe_wif_d2_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000106;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_d2_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000107;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d2_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000108;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d2_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* wpe_wif_d3_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000206;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_d3_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000207;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d3_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000208;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_d3_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* mcrp_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000306;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[mcrp_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000307;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[mcrp_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000308;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[mcrp_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

}

void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *dipRegBA = 0L;
	unsigned int i;
	unsigned int DMADdbSel = DIP_DMA_DBG_SEL;
	unsigned int DMADbgOut = DIP_DMA_DBG_PORT;
	unsigned int CtlDdbSel = DIP_DBG_SEL;
	unsigned int CtlDbgOut = DIP_DBG_OUT;
	char DMANrPort = 0;

	pr_info("%s: +\n", __func__);

	/* 0x15100000~ */
	dipRegBA = gdipRegBA[0];
	g_RegBaseAddr = DIP_TOP_ADDR;

	/* DL debug data */
	imgsys_dip_dump_dl(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);

	/* ctrl reg */
	for (i = TOP_CTL_OFT; i <= (TOP_CTL_OFT + TOP_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* DMA reg */
	for (i = DMATOP_OFT; i <= (DMATOP_OFT + DMATOP_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* NR3D */
	for (i = NR3D_CTL_OFT; i <= (NR3D_CTL_OFT + NR3D_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SNRS */
	for (i = SNRS_CTL_OFT; i <= (SNRS_CTL_OFT + SNRS_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* UNP_D1~C20_D1 */
	for (i = UNP_D1_CTL_OFT; i <= (UNP_D1_CTL_OFT + UNP_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SMT_D1~PAK_D2 */
	for (i = SMT_D1_CTL_OFT; i <= (SMT_D1_CTL_OFT + SMT_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_TOP_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	/* 0x15154000~ */
	dipRegBA = gdipRegBA[1];
	/* SNR_D1~PCRP_D16*/
	for (i = SNR_D1_CTL_OFT; i <= (SNR_D1_CTL_OFT + SNR_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* EE_D1~URZS2T_D5*/
	for (i = EE_D1_CTL_OFT; i <= (EE_D1_CTL_OFT + EE_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_BCE */
	for (i = TNC_BCE_CTL_OFT; i <= (TNC_BCE_CTL_OFT + TNC_BCE_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_TILE */
	for (i = TNC_TILE_CTL_OFT; i <= (TNC_TILE_CTL_OFT + TNC_TILE_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_C2G~TNC_TNC_CTL */
	for (i = TNC_C2G_CTL_OFT; i <= (TNC_C2G_CTL_OFT + TNC_C2G_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* TNC_C3D */
	for (i = TNC_C3D_CTL_OFT; i <= (TNC_C3D_CTL_OFT + TNC_C3D_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR1_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	/* 0x15161000~ */
	dipRegBA = gdipRegBA[2];
	/* VIPI_D1~SMTCI_D9 */
	for (i = VIPI_D1_CTL_OFT; i <= (VIPI_D1_CTL_OFT + VIPI_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SNRCSI_D1~SMTO_D9 */
	for (i = SNRCSI_D1_CTL_OFT; i <= (SNRCSI_D1_CTL_OFT + SNRCSI_D1_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* SMTCO_D4~DRZS8T_D1 */
	for (i = SMTCO_D4_CTL_OFT; i <= (SMTCO_D4_CTL_OFT + SMTCO_D4_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}
	/* DRZH2N_D2 */
	for (i = DRZH2N_D2_CTL_OFT; i <= (DRZH2N_D2_CTL_OFT + DRZH2N_D2_CTL_SZ); i += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(DIP_NR2_ADDR + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	/* DMA_TOP debug data */
	DMANrPort = 0;
	dipRegBA = gdipRegBA[0];
	g_RegBaseAddr = DIP_TOP_ADDR;
	imgsys_dip_dump_dma(imgsys_dev, dipRegBA, DMADdbSel, DMADbgOut, DMANrPort);
	/* DMA_NR debug data */
	DMANrPort = 1;
	dipRegBA = gdipRegBA[2];
	g_RegBaseAddr = DIP_NR2_ADDR;
	imgsys_dip_dump_dma(imgsys_dev, dipRegBA, DMADdbSel, DMADbgOut, DMANrPort);


	pr_info("%s: -\n", __func__);

}

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdipRegBA[i]);
		gdipRegBA[i] = 0L;
	}
}
