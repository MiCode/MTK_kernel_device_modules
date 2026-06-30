// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include "cmdq-util.h"
#include <linux/module.h>
#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include "cmdq_chist_backup.h"
#include "mtk_log.h"


// chist related regs
#define DISP_CHIST0 0x320D0000
#define DISP_CHIST1 0x320E0000

#define DISP_CHIST_HIST_CH_CFG1(HwId) (DISP_CHIST0 + (HwId) * 0x10000 + 0x0510)
#define DISP_CHIST_APB_READ(HwId) (DISP_CHIST0 + (HwId) * 0x10000 + 0x0600)
#define DISP_CHIST_SRAM_R_IF(HwId) (DISP_CHIST0 + (HwId) * 0x10000 + 0x0680)

#define DISP_CHIST_CH_WIN_X_MAIN(HwId, chanId) \
		(DISP_CHIST0 + (HwId) * 0x10000 + \
		 (chanId) * 0x10 + 0x0460)
#define DISP_CHIST_CH_WIN_Y_MAIN(HwId, chanId) \
		(DISP_CHIST0 + (HwId) * 0x10000 + \
		 (chanId) * 0x10 + 0x0464)
#define DISP_CHIST_CH_BLOCK_INFO(HwId, chanId) \
		(DISP_CHIST0 + (HwId) * 0x10000 + \
		 (chanId) * 0x10 + 0x0468)

static unsigned int cmdq_get_backup_reg(enum MEM_LAYOUT_ENUM layout)
{
	switch (layout) {
	case MEM_LAYOUT_CHIST0_HIST_CH_CFG1:
		return DISP_CHIST_HIST_CH_CFG1(0);
	case MEM_LAYOUT_CHIST0_CH0_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 0);
	case MEM_LAYOUT_CHIST0_CH0_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 0);
	case MEM_LAYOUT_CHIST0_CH0_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 0);
	case MEM_LAYOUT_CHIST0_CH1_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 1);
	case MEM_LAYOUT_CHIST0_CH1_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 1);
	case MEM_LAYOUT_CHIST0_CH1_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 1);
	case MEM_LAYOUT_CHIST0_CH2_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 2);
	case MEM_LAYOUT_CHIST0_CH2_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 2);
	case MEM_LAYOUT_CHIST0_CH2_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 2);
	case MEM_LAYOUT_CHIST0_CH3_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 3);
	case MEM_LAYOUT_CHIST0_CH3_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 3);
	case MEM_LAYOUT_CHIST0_CH3_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 3);
	case MEM_LAYOUT_CHIST0_CH4_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 4);
	case MEM_LAYOUT_CHIST0_CH4_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 4);
	case MEM_LAYOUT_CHIST0_CH4_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 4);
	case MEM_LAYOUT_CHIST0_CH5_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 5);
	case MEM_LAYOUT_CHIST0_CH5_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 5);
	case MEM_LAYOUT_CHIST0_CH5_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 5);
	case MEM_LAYOUT_CHIST0_CH6_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(0, 6);
	case MEM_LAYOUT_CHIST0_CH6_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(0, 6);
	case MEM_LAYOUT_CHIST0_CH6_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(0, 6);

	case MEM_LAYOUT_CHIST0_SRAM_R_IF: // max_bin:
		return DISP_CHIST_SRAM_R_IF(0);

	// chist 1
	case MEM_LAYOUT_CHIST1_HIST_CH_CFG1:
		return DISP_CHIST_HIST_CH_CFG1(1);
	case MEM_LAYOUT_CHIST1_CH0_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 0);
	case MEM_LAYOUT_CHIST1_CH0_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 0);
	case MEM_LAYOUT_CHIST1_CH0_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 0);
	case MEM_LAYOUT_CHIST1_CH1_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 1);
	case MEM_LAYOUT_CHIST1_CH1_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 1);
	case MEM_LAYOUT_CHIST1_CH1_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 1);
	case MEM_LAYOUT_CHIST1_CH2_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 2);
	case MEM_LAYOUT_CHIST1_CH2_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 2);
	case MEM_LAYOUT_CHIST1_CH2_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 2);
	case MEM_LAYOUT_CHIST1_CH3_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 3);
	case MEM_LAYOUT_CHIST1_CH3_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 3);
	case MEM_LAYOUT_CHIST1_CH3_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 3);
	case MEM_LAYOUT_CHIST1_CH4_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 4);
	case MEM_LAYOUT_CHIST1_CH4_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 4);
	case MEM_LAYOUT_CHIST1_CH4_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 4);
	case MEM_LAYOUT_CHIST1_CH5_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 5);
	case MEM_LAYOUT_CHIST1_CH5_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 5);
	case MEM_LAYOUT_CHIST1_CH5_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 5);
	case MEM_LAYOUT_CHIST1_CH6_WIN_X_MAIN:
		return DISP_CHIST_CH_WIN_X_MAIN(1, 6);
	case MEM_LAYOUT_CHIST1_CH6_WIN_Y_MAIN:
		return DISP_CHIST_CH_WIN_Y_MAIN(1, 6);
	case MEM_LAYOUT_CHIST1_CH6_BLOCK_INFO:
		return DISP_CHIST_CH_BLOCK_INFO(1, 6);

	case MEM_LAYOUT_CHIST1_SRAM_R_IF: // max_bin:
		return DISP_CHIST_SRAM_R_IF(1);

	default:
		pr_info("error: invalid layout:%d\n", layout);
	}
	return 0;
}

dma_addr_t cmdq_slot_get_pa(const struct slot_info *slot, enum MEM_LAYOUT_ENUM layout)
{
	return slot->pa + layout * 4;
}

unsigned int *cmdq_slot_get_va(const struct slot_info *slot, enum MEM_LAYOUT_ENUM layout)
{
	return slot->va;
}

unsigned int cmdq_slot_get_value(const struct slot_info *slot, enum MEM_LAYOUT_ENUM layout)
{
	return slot->va[layout];
}

int cmdq_slot_get_size(const struct slot_info *slot)
{
	return slot->size;
}

struct slot_info *cmdq_slot_alloc(struct cmdq_client *clt)
{
	struct slot_info *slot = NULL;

	slot = vmalloc(sizeof(struct slot_info));
	slot->va = (unsigned int *)cmdq_mbox_buf_alloc(clt, &(slot->pa));
	//slot->va = (unsigned int *)dma_alloc_coherent(clt->share_dev,
	//CMDQ_BUF_ALLOC_SIZE, &(slot->pa), GFP_KERNEL);
	if (!slot->va || !slot->pa) {
		memset(slot, 0, sizeof(*slot));
		pr_err("allocate cmdq backup slot fail\n");
		vfree(slot);
		return NULL;
	}
	// allocate success.
	slot->size = CMDQ_BUF_ALLOC_SIZE;
	slot->client = clt;

	return slot;
}

void cmdq_slot_free(struct slot_info *slot)
{
	cmdq_mbox_buf_free(slot->client, slot->va, slot->pa);

	vfree(slot);
}

int cmdq_pkt_backup_chist(struct cmdq_pkt *pkt, const struct slot_info *slot)
{
	enum MEM_LAYOUT_ENUM layout = MEM_LAYOUT_CHIST0_HIST_CH_CFG1;
	int i = 0;
	int channel = 0;

	// dump chist0
	for (layout = MEM_LAYOUT_CHIST0_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST0_CH6_BLOCK_INFO;
		layout++) {
		cmdq_pkt_mem_move(pkt, NULL,
			cmdq_get_backup_reg(layout),
			cmdq_slot_get_pa(slot, layout),
			CMDQ_THR_SPR_IDX1);
	}
	// dump chist0 histograme
	for (channel = 0; channel < MAX_CHANNEL; channel++) {
		// set histogram auto inc
		cmdq_pkt_write(pkt, NULL, DISP_CHIST_APB_READ(0), 0x30 | channel, ~0);
		for (i = 0; i < MAX_BINS; i++)
			cmdq_pkt_mem_move(pkt, NULL,
				cmdq_get_backup_reg(MEM_LAYOUT_CHIST0_SRAM_R_IF),
				cmdq_slot_get_pa(slot,
				MEM_LAYOUT_CHIST0_SRAM_R_IF + i + channel * MAX_BINS),
				CMDQ_THR_SPR_IDX1);
	}

	// dump chist1
	for (layout = MEM_LAYOUT_CHIST1_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST1_CH6_BLOCK_INFO;
		layout++) {
		cmdq_pkt_mem_move(pkt, NULL,
			cmdq_get_backup_reg(layout),
			cmdq_slot_get_pa(slot, layout),
			CMDQ_THR_SPR_IDX1);
	}
	// dump chist1 histograme
	for (channel = 0; channel < MAX_CHANNEL; channel++) {
		// set histogram auto inc
		cmdq_pkt_write(pkt, NULL, DISP_CHIST_APB_READ(1), 0x30 | channel, ~0);
		for (i = 0; i < MAX_BINS; i++)
			cmdq_pkt_mem_move(pkt, NULL,
				cmdq_get_backup_reg(MEM_LAYOUT_CHIST1_SRAM_R_IF),
				cmdq_slot_get_pa(slot,
				MEM_LAYOUT_CHIST1_SRAM_R_IF + i + channel * MAX_BINS),
				CMDQ_THR_SPR_IDX1);
	}
	return 0;
}

int cmdq_pkt_backup_chist_region_info(struct cmdq_pkt *pkt, const struct slot_info *slot)
{
	enum MEM_LAYOUT_ENUM layout = MEM_LAYOUT_CHIST0_HIST_CH_CFG1;
	int i = 0;
	int channel = 0;

	// dump chist0
	for (layout = MEM_LAYOUT_CHIST0_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST0_CH6_BLOCK_INFO;
		layout++) {
		cmdq_pkt_mem_move(pkt, NULL,
			cmdq_get_backup_reg(layout),
			cmdq_slot_get_pa(slot, layout),
			CMDQ_THR_SPR_IDX1);
	}

	// dump chist1
	for (layout = MEM_LAYOUT_CHIST1_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST1_CH6_BLOCK_INFO;
		layout++) {
		cmdq_pkt_mem_move(pkt, NULL,
			cmdq_get_backup_reg(layout),
			cmdq_slot_get_pa(slot, layout),
			CMDQ_THR_SPR_IDX1);
	}

	return 0;
}

int cmdq_pkt_backup_chist_hist(struct cmdq_pkt *pkt, const struct slot_info *slot)
{
	int i = 0;
	int channel = 0;

	// dump chist0 histograme
	for (channel = 0; channel < MAX_CHANNEL; channel++) {
		// set histogram auto inc
		cmdq_pkt_write(pkt, NULL, DISP_CHIST_APB_READ(0), 0x30 | channel, ~0);
		for (i = 0; i < MAX_BINS; i++)
			cmdq_pkt_mem_move(pkt, NULL,
				cmdq_get_backup_reg(MEM_LAYOUT_CHIST0_SRAM_R_IF),
				cmdq_slot_get_pa(slot,
				MEM_LAYOUT_CHIST0_SRAM_R_IF + i + channel * MAX_BINS),
				CMDQ_THR_SPR_IDX1);
	}

	// dump chist1 histograme
	for (channel = 0; channel < MAX_CHANNEL; channel++) {
		// set histogram auto inc
		cmdq_pkt_write(pkt, NULL, DISP_CHIST_APB_READ(1), 0x30 | channel, ~0);
		for (i = 0; i < MAX_BINS; i++)
			cmdq_pkt_mem_move(pkt, NULL,
				cmdq_get_backup_reg(MEM_LAYOUT_CHIST1_SRAM_R_IF),
				cmdq_slot_get_pa(slot,
				MEM_LAYOUT_CHIST1_SRAM_R_IF + i + channel * MAX_BINS),
				CMDQ_THR_SPR_IDX1);
	}
	return 0;
}



static void cmdq_flush_async_cb(struct cmdq_cb_data data)
{
	struct slot_info *slot = (struct slot_info *)data.data;
	enum MEM_LAYOUT_ENUM layout;
	int i = 0;

	pr_info("cmdq execute done, dump chist regs\n");
	for (layout = MEM_LAYOUT_CHIST0_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST0_CH6_BLOCK_INFO;
		layout++)
		pr_info("layout:%d value:0x%08x\n", layout, cmdq_slot_get_value(slot, layout));
	for (i = 0; i < MAX_BINS; i++)
		pr_info("chist0 hist[%02d], value:0x%08x\n",
		i, cmdq_slot_get_value(slot, MEM_LAYOUT_CHIST0_SRAM_R_IF + i));

	for (layout = MEM_LAYOUT_CHIST1_HIST_CH_CFG1;
		layout <= MEM_LAYOUT_CHIST1_CH6_BLOCK_INFO;
		layout++)
		pr_info("layout:%d value:0x%08x\n", layout, cmdq_slot_get_value(slot, layout));
	for (i = 0; i < MAX_BINS; i++)
		pr_info("chist1 hist[%02d], value:0x%08x\n",
		i, cmdq_slot_get_value(slot, MEM_LAYOUT_CHIST1_SRAM_R_IF + i));
	pr_info("dump chist end\n");

	// free backup slot
	cmdq_slot_free(slot);
}

int cmdq_backup_chist(struct cmdq_client *clt)
{
	struct cmdq_pkt *pkt = NULL;
	struct slot_info *slot = NULL;

	// allocate cmdq backup buffer
	slot = cmdq_slot_alloc(clt);
	if (slot == NULL)
		return -1;
	// allocate cmdq pkt.
	pkt = cmdq_pkt_create(clt);
	//cmdq_pkt_backup_chist(pkt, slot);
	cmdq_pkt_flush_threaded(pkt, cmdq_flush_async_cb, slot);

	return 0;
}
EXPORT_SYMBOL(cmdq_backup_chist);
