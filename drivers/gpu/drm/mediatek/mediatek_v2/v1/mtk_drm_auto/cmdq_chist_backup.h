/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __CMDQ_CHIST_BACKUP_H__
#define __CMDQ_CHIST_BACKUP_H__
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

// chist histogram count
#define MAX_BINS 32  // histogram count for each channel
#define MAX_CHANNEL 7 // chist has 7 channels

enum MEM_LAYOUT_ENUM {
	// chist 0
	MEM_LAYOUT_CHIST0_HIST_CH_CFG1 = 0,
	MEM_LAYOUT_CHIST0_CH0_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH0_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH0_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH1_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH1_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH1_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH2_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH2_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH2_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH3_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH3_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH3_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH4_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH4_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH4_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH5_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH5_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH5_BLOCK_INFO,
	MEM_LAYOUT_CHIST0_CH6_WIN_X_MAIN,
	MEM_LAYOUT_CHIST0_CH6_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST0_CH6_BLOCK_INFO,

	// chist 1
	MEM_LAYOUT_CHIST1_HIST_CH_CFG1,
	MEM_LAYOUT_CHIST1_CH0_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH0_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH0_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH1_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH1_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH1_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH2_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH2_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH2_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH3_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH3_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH3_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH4_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH4_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH4_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH5_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH5_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH5_BLOCK_INFO,
	MEM_LAYOUT_CHIST1_CH6_WIN_X_MAIN,
	MEM_LAYOUT_CHIST1_CH6_WIN_Y_MAIN,
	MEM_LAYOUT_CHIST1_CH6_BLOCK_INFO,

	MEM_LAYOUT_CHIST0_SRAM_R_IF, // max_bins
	MEM_LAYOUT_CHIST1_SRAM_R_IF =
	MEM_LAYOUT_CHIST0_SRAM_R_IF + MAX_BINS * MAX_CHANNEL, // max_bins

	MEM_LAYOUT_MAX = MEM_LAYOUT_CHIST1_SRAM_R_IF + MAX_BINS * MAX_CHANNEL,
};

struct slot_info {
	unsigned int *va;
	dma_addr_t pa;
	int size;
	struct cmdq_client *client;
	struct cmdq_pkt *pkt;
};

int cmdq_backup_chist(struct cmdq_client *clt);
int cmdq_pkt_backup_chist(struct cmdq_pkt *pkt, const struct slot_info *slot);
int cmdq_pkt_backup_chist_region_info(struct cmdq_pkt *pkt, const struct slot_info *slot);
int cmdq_pkt_backup_chist_hist(struct cmdq_pkt *pkt, const struct slot_info *slot);
struct slot_info *cmdq_slot_alloc(struct cmdq_client *clt);
void cmdq_slot_free(struct slot_info *slot);
int cmdq_slot_get_size(const struct slot_info *slot);
unsigned int cmdq_slot_get_value(const struct slot_info *slot, enum MEM_LAYOUT_ENUM layout);
unsigned int *cmdq_slot_get_va(const struct slot_info *slot, enum MEM_LAYOUT_ENUM layout);
#endif
