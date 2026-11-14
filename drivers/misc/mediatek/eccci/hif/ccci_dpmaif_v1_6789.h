/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */
#ifndef __CCCI_DPMAIF_V1_MT6789_H__
#define __CCCI_DPMAIF_V1_MT6789_H__
#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include "ccci_debug.h"
#include "ccci_dpmaif_reg_com.h"
#include "ccci_dpmaif_drv_com.h"
#include "ccci_hif_internal.h"
#include "ccci_hif_ccif.h"
#include "ccmni.h"
#include "ccci_dpmaif_com.h"

/* just for moet use */

struct drb_queue_info {
	unsigned int rd;
	unsigned int wr;
	unsigned int rel;
	unsigned int sz;
};

#define DPMA_UL_QUEUE_NUM   3
#define DPMA_UL_Q0_SIZE     1024
#define DPMA_UL_Q1_SIZE     1024
#define DPMA_UL_Q2_SIZE     1024

#define DPMA_SKB_DATA_LEN   1600

struct dpmaif_ctrl_v1_mt6789 {
	struct workqueue_struct *smem_worker;
	struct delayed_work      smem_drb_work;

	void                    *smem_base_vir;
	phys_addr_t              smem_base_phy;
	unsigned int             smem_size;

	struct drb_queue_info   *smem_drb_qinfo[DPMA_UL_QUEUE_NUM];
	struct dpmaif_drb_pd    *smem_drb_qbase[DPMA_UL_QUEUE_NUM];

	struct drb_queue_info    smem_drb_qbuf_inf[DPMA_UL_QUEUE_NUM];
	void                    *smem_drb_qbuf_vir[DPMA_UL_QUEUE_NUM];
	phys_addr_t              smem_drb_qbuf_phy[DPMA_UL_QUEUE_NUM];

	struct dpmaif_drb_skb   *dpma_drb_qskb[DPMA_UL_QUEUE_NUM];
};

extern int register_ccif_irq_cb(unsigned char user_id, void (*cb_func)(unsigned char user_id));
extern int ccif_mask_setting(unsigned char user_id, unsigned char mask_set);
extern phys_addr_t ccci_get_md_view_phy_addr_by_user_id(enum SMEM_USER_ID user_id);
extern void dpmaif_txq_set_budget_v1_mt6789(struct dpmaif_tx_queue *txq);
extern void dpmaif_smem_tx_stop(void);
extern int dpmaif_tx_send_skb_to_smem(struct ccmni_tx_para_info *tx_info);
extern int dpmaif_tx_sw_solution_init(void);
extern void drv1_hw_reset_for_6789(void);
#endif
