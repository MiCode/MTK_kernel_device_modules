/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DPMA_BAT_H__
#define __CCCI_DPMA_BAT_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>


#include "ccci_dpmaif_com.h"


#define MAX_ALLOC_BAT_CNT (0xFFFF)
#define MAX_ALLOC_BAT_CNT_FROM_MD (100000)

#define MIN_ALLOC_SKB_CNT (2000)
#define MIN_ALLOC_FRG_CNT (2000)
#define MIN_ALLOC_SKB_TBL_CNT (100)
#define MIN_ALLOC_FRG_TBL_CNT (100)
/* In real life use, when bat_alloc thread exits, 90% of alloc cnt falls
 * within the range [1,33], MD lhif driver thinks when bat alloc cnt is
 * less than 61, bat-not-enough isr will pop; based on the two reason,
 * choose 30 as threshold which bat_alloc should continue
 */
#define BAT_CONTINUE_THR  (30)
#define MIN_BAT_UPD_THR  (100)


int ccci_dpmaif_bat_init(struct device *dev);

int ccci_dpmaif_bat_late_init(void);

int ccci_dpmaif_bat_start(void);

void ccci_dpmaif_bat_stop(void);

inline void ccci_dpmaif_bat_wakeup_thread(void);
inline void ccci_dpmaif_skb_wakeup_thread(void);
inline int ccci_dpmaif_pit_need_wake_up_bat(struct dpmaif_rx_queue *rxq, unsigned short pit_rd_idx);
extern atomic_t g_bat_alloc_running;
extern atomic_t g_alloc_skb_threshold;
extern unsigned int g_alloc_frg_threshold;
extern unsigned int g_alloc_skb_tbl_threshold;
extern unsigned int g_alloc_frg_tbl_threshold;
extern unsigned int g_max_bat_skb_cnt_for_md;
#ifdef RX_PAGE_POOL
extern atomic_t g_create_another_pp;
#endif
#endif /* __CCCI_DPMA_BAT_H__ */

