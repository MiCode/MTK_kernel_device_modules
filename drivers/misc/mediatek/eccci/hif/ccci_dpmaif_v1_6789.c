// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

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
#include "ccci_dpmaif_reg_v1.h"
#include "ccci_dpmaif_reg_com.h"
#include "ccci_dpmaif_bat.h"
#include "ccci_dpmaif_drv_com.h"
#include "ccci_hif_internal.h"
#include "ccci_dpmaif_v1_6789.h"
#include "ccmni.h"
#define TAG "dpmf"
static struct dpmaif_ctrl_v1_mt6789 *g_dpmaif_ctrl_v1_mt6789;

static unsigned int g_smem_drb_qsize[DPMA_UL_QUEUE_NUM] = {
		DPMA_UL_Q0_SIZE,
		DPMA_UL_Q1_SIZE,
		DPMA_UL_Q2_SIZE,
};

void dpmaif_txq_set_budget_v1_mt6789(struct dpmaif_tx_queue *txq)
{
	int qno;

	if (g_dpmaif_ctrl->tx_sw_solution) {
		qno = txq->index;
		if (qno >= DPMA_UL_QUEUE_NUM)
			qno = DPMA_UL_QUEUE_NUM - 1;
		atomic_set(&(g_dpmaif_ctrl->txq[qno].txq_budget), g_smem_drb_qsize[qno]);
	}
}

static int dpmaif_smem_release_tx_buffer(unsigned int qno)
{
	struct drb_queue_info *drb_qinfo = g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[qno];
	struct dpmaif_drb_pd  *smem_drb_base = g_dpmaif_ctrl_v1_mt6789->smem_drb_qbase[qno];
	struct dpmaif_drb_skb *smem_drb_skb  = g_dpmaif_ctrl_v1_mt6789->dpma_drb_qskb[qno];
	struct dpmaif_tx_queue *txq = &g_dpmaif_ctrl->txq[qno];
	int i, rel_cnt;
	unsigned int cur_idx;
	struct dpmaif_drb_pd  *drb_pd;
	struct dpmaif_drb_skb *drb_skb;

	rel_cnt = get_ringbuf_release_cnt(drb_qinfo->sz, drb_qinfo->rel, drb_qinfo->rd);
	if (rel_cnt <= 0)
		return 0;

	cur_idx = drb_qinfo->rel;

	for (i = 0; i < rel_cnt; i++) {
		drb_pd = &smem_drb_base[cur_idx];
		if (drb_pd->dtyp == DES_DTYP_PD && drb_pd->c_bit == 0) {
			drb_skb = &smem_drb_skb[cur_idx];
			if (drb_skb->skb == NULL) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: q:%d; i: %d; qinfo: rel/rd/wr(%u/%u/%u)\n",
					__func__, qno, i,
					drb_qinfo->rel, drb_qinfo->rd, drb_qinfo->wr);
				return -1;
			}
			if (atomic_cmpxchg(&g_dpmaif_ctrl->wakeup_src, 1, 0) == 1) {
				CCCI_NORMAL_LOG(0, TAG,
					"[%s] DPMA_MD wakeup source: txq%d.\n",
					__func__, txq->index);
				dpmaif_handle_wakeup_skb(0, drb_skb->skb);
			}
			dev_kfree_skb_any(drb_skb->skb);
			drb_skb->skb = NULL;
			if (g_debug_flags & DEBUG_TX_DONE_SKB) {
				struct debug_tx_done_skb_hdr hdr;

				hdr.type = TYPE_TX_DONE_SKB_ID;
				hdr.qidx = qno;
				hdr.time = (unsigned int)(local_clock() >> 16);
				hdr.rel = cur_idx;
				ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
			}
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
			g_dpmaif_ctrl->tx_tfc_pkgs[txq->index]++;
#endif
		}

		cur_idx = get_ringbuf_next_idx(drb_qinfo->sz, cur_idx, 1);
		atomic_inc(&txq->txq_budget);

		if (atomic_read(&txq->txq_ccmni_stop_counter)) {
			if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP)) {
				if (atomic_read(&txq->txq_budget) > (drb_qinfo->sz / 8))
					dpmaif_start_dev_queue(txq);
			}
		}


	}

	drb_qinfo->rel = cur_idx;

	return rel_cnt;
}

void dpmaif_smem_tx_stop(void)
{
	int i;

	/* flush work */
	cancel_delayed_work(&g_dpmaif_ctrl_v1_mt6789->smem_drb_work);
	flush_delayed_work(&g_dpmaif_ctrl_v1_mt6789->smem_drb_work);

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		dpmaif_smem_release_tx_buffer(i);

		g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->rel = 0;
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->rd = 0;
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->wr = 0;

		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf[i].rel = 0;
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf[i].rd = 0;
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf[i].wr = 0;
	}
}

static void dpmaif_smem_tx_irq_func(unsigned char user_id)
{
	ccif_mask_setting(ID_CCIF_USER_DATA, 1);
	if (g_debug_flags & DEBUG_RXTX_ISR) {
		struct debug_rxtx_isr_hdr hdr = {0};

		hdr.type = TYPE_RXTX_ISR_ID;
		hdr.qidx = 0;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.rxsr = 0;
		hdr.rxmr = 0;
		hdr.txsr = 1;
		hdr.txmr = 0;
		hdr.l1sr = 0;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}
	queue_delayed_work(g_dpmaif_ctrl_v1_mt6789->smem_worker,
				&g_dpmaif_ctrl_v1_mt6789->smem_drb_work,
				msecs_to_jiffies(5));
}

static inline int dpmaif_set_skb_data_to_smem_drb(struct dpmaif_tx_queue *txq,
	unsigned int send_cnt, struct ccmni_tx_para_info *tx_info)
{
	unsigned int cur_idx = 0, is_frag = 0, c_bit = 0, wt_cnt = 0, data_len = 0, payload_cnt = 0;
	unsigned int qno = tx_info->hw_qno;
	struct drb_queue_info *drb_qinfo     = g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[qno];
	struct dpmaif_drb_pd  *smem_drb_base = g_dpmaif_ctrl_v1_mt6789->smem_drb_qbase[qno];
	struct dpmaif_drb_skb *smem_drb_skb  = g_dpmaif_ctrl_v1_mt6789->dpma_drb_qskb[qno];
	struct dpmaif_drb_msg *drb_msg = NULL;
	struct dpmaif_drb_pd  *drb_pd = NULL;
	struct dpmaif_drb_skb *drb_skb = NULL;
	struct drb_queue_info *drb_qbuf_inf = &g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf[qno];
	struct skb_shared_info *shinfo = NULL;
	void *data_addr = NULL;
	void *smem_data_addr = NULL;
	dma_addr_t phy_addr = 0;
	struct sk_buff *skb = tx_info->skb;

	if (!drb_qinfo) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: drb_qnifo is null!\n", __func__);
		return -ENODEV;
	}
	if (!smem_drb_base) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: smem_drb_base is null!\n", __func__);
		return -ENODEV;
	}
	if (!smem_drb_skb) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: smem_drb_skb is null!\n", __func__);
		return -ENODEV;
	}
	if (!drb_qbuf_inf) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: drb_qbuf_inf is null!\n", __func__);
		return -ENODEV;
	}

	cur_idx = drb_qinfo->wr;
	drb_msg = (struct dpmaif_drb_msg *)(&smem_drb_base[cur_idx]);
	drb_msg->dtyp = DES_DTYP_MSG;
	drb_msg->c_bit = 1;
	drb_msg->packet_len = skb->len;
	drb_msg->count_l = tx_info->count_l;
	drb_msg->channel_id = tx_info->ccmni_idx;
	drb_msg->network_type = tx_info->network_type;
	drb_msg->ipv4 = 0;
	drb_msg->l4 = 0;

	drb_skb = &smem_drb_skb[cur_idx];
	drb_skb->skb = skb;
	drb_skb->phy_addr = 0;
	drb_skb->data_len = 0;
	drb_skb->drb_idx = cur_idx;
	drb_skb->is_msg = 1;
	drb_skb->is_frag = 0;
	drb_skb->is_last_one = 0;

	cur_idx = get_ringbuf_next_idx(drb_qinfo->sz, cur_idx, 1);

	payload_cnt = send_cnt - 1;
	shinfo = skb_shinfo(skb);

	for (wt_cnt = 0; wt_cnt < payload_cnt; wt_cnt++) {
		if (wt_cnt == 0) {
			data_len = skb_headlen(skb);
			data_addr = skb->data;
			is_frag = 0;

		} else {
			skb_frag_t *frag = shinfo->frags + (wt_cnt - 1);

			data_len = skb_frag_size(frag);
			data_addr = skb_frag_address(frag);
			is_frag = 1;
		}

		if (wt_cnt == payload_cnt - 1)
			c_bit = 0;
		else
			c_bit = 1;

		smem_data_addr = g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_vir[qno] +
						(DPMA_SKB_DATA_LEN * drb_qbuf_inf->wr);
		phy_addr = g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_phy[qno] +
						(DPMA_SKB_DATA_LEN * drb_qbuf_inf->wr);
		if (data_addr)
			memcpy_toio(smem_data_addr, data_addr, data_len);

		drb_qbuf_inf->wr = get_ringbuf_next_idx(drb_qbuf_inf->sz, drb_qbuf_inf->wr, 1);

		drb_pd = &smem_drb_base[cur_idx];
		drb_pd->dtyp = DES_DTYP_PD;
		drb_pd->c_bit = c_bit;
		drb_pd->data_len = data_len;
		drb_pd->p_data_addr = phy_addr & 0xFFFFFFFF;
		drb_pd->data_addr_ext = (phy_addr >> 32) & 0xFF;

		drb_skb = &smem_drb_skb[cur_idx];
		drb_skb->skb = skb;
		drb_skb->phy_addr = phy_addr;
		drb_skb->data_len = data_len;
		drb_skb->drb_idx = cur_idx;
		drb_skb->is_msg = 0;
		drb_skb->is_frag = is_frag;
		drb_skb->is_last_one = (c_bit == 0 ? 1 : 0);
		if (g_debug_flags & DEBUG_TX_SEND_SKB) {
			struct debug_tx_send_skb_hdr hdr = {0};

			hdr.type      = TYPE_TX_SEND_SKB_ID;
			hdr.qidx      = qno;
			hdr.net_type  = tx_info->network_type;
			hdr.time      = (unsigned int)(local_clock() >> 16);
			hdr.wr        = is_frag ? (cur_idx | 0x8000) : cur_idx;
			hdr.ipid      = ((struct iphdr *)skb->data)->id;
			hdr.len       = data_len;
			hdr.count_l   = tx_info->count_l;
			hdr.ccmni_idx = tx_info->ccmni_idx;
			hdr.queue_idx = tx_info->hw_qno;
			hdr.budget    = atomic_read(&txq->txq_budget) - send_cnt;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}
		cur_idx = get_ringbuf_next_idx(drb_qinfo->sz, cur_idx, 1);
	}

	atomic_sub(send_cnt, &txq->txq_budget);
	drb_qinfo->wr = cur_idx;

	/* 3.3 submit drb descriptor*/
	wmb();

	dpmaif_write32(g_dpmaif_ctrl->pd_md_misc_base, 0x1C, 0x03);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	g_dpmaif_ctrl->tx_pre_tfc_pkgs[txq->index]++;
#endif
	return 0;
}

static int dpmaif_handle_skb_data(struct dpmaif_tx_queue *txq, struct ccmni_tx_para_info *tx_info)
{
	unsigned int qno = tx_info->hw_qno;
	struct drb_queue_info *drb_qinfo = g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[qno];
	struct skb_shared_info *info = NULL;
	unsigned int remain_cnt, send_cnt = 0, payload_cnt = 0;
	unsigned long flags;
	int ret = 0;

	if (!drb_qinfo) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: drb_qinfo is null!\n", __func__);
		return -ENODEV;
	}
	info = skb_shinfo(tx_info->skb);

	if (info->frag_list != NULL)
		CCCI_ERROR_LOG(0, TAG, "[%s] error: skb frag_list not supported!\n", __func__);

	payload_cnt = info->nr_frags + 1;
	/* nr_frags: frag cnt, 1: skb->data, 1: msg drb */
	send_cnt = payload_cnt + 1;

	spin_lock_irqsave(&txq->txq_lock, flags);

	remain_cnt = get_ringbuf_free_cnt(drb_qinfo->sz, drb_qinfo->rel, drb_qinfo->wr);
	if (remain_cnt < send_cnt) {
		/* buffer check: full */
		if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP))
			dpmaif_stop_dev_queue(txq, tx_info->ccmni_idx,
				tx_info->hw_qno, g_smem_drb_qsize[qno]>>3);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
		txq->busy_count++;
#endif
		ret = -EBUSY;
		goto __EXIT_FUN;
	}

	ret = dpmaif_set_skb_data_to_smem_drb(txq, send_cnt, tx_info);

__EXIT_FUN:
	spin_unlock_irqrestore(&txq->txq_lock, flags);
	return ret;
}

int dpmaif_tx_send_skb_to_smem(struct ccmni_tx_para_info *tx_info)
{
	struct dpmaif_tx_queue *txq = NULL;
	unsigned int qno = tx_info->hw_qno;
	int ret = 0;

	if (!tx_info->skb)
		return 0;

	if (dpmaif_wait_resume_done() < 0)
		return -EBUSY;

	if (qno >= DPMA_UL_QUEUE_NUM) {
		qno = DPMA_UL_QUEUE_NUM - 1;
		tx_info->hw_qno = qno;
	}
	txq = &g_dpmaif_ctrl->txq[qno];

	if (g_dpmaif_ctrl->dpmaif_state != DPMAIF_STATE_PWRON)
		return -CCCI_ERR_HIF_NOT_POWER_ON;

	if (atomic_read(&g_tx_busy_assert_on)) {
		if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP))
			dpmaif_stop_dev_queue(txq, tx_info->ccmni_idx,
				tx_info->hw_qno, g_smem_drb_qsize[qno]);
		return HW_REG_CHK_FAIL;
	}

	atomic_set(&txq->txq_processing, 1);
	smp_mb(); /* for cpu exec. */
	if (txq->started != true) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto __EXIT_FUN;
	}

	ret = dpmaif_handle_skb_data(txq, tx_info);
	if (ret)
		goto __EXIT_FUN;


__EXIT_FUN:
	atomic_set(&txq->txq_processing, 0);

	return ret;
}

static int dpmaif_alloc_smem_to_drb(void)
{
	int i, len, pos = 0;
	unsigned int size = g_dpmaif_ctrl_v1_mt6789->smem_size;

	memset(g_dpmaif_ctrl_v1_mt6789->smem_base_vir, 0, size);
	memset(g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf, 0, sizeof(g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf));

	len = sizeof(struct drb_queue_info) * DPMA_UL_QUEUE_NUM;
	if ((pos + len) > size) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: smem size too small.(%d/%u)\n",
			__func__, pos + len, size);
		return -1;
	}

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i] = (struct drb_queue_info *)
							(g_dpmaif_ctrl_v1_mt6789->smem_base_vir + pos);
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->sz = g_smem_drb_qsize[i];
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_inf[i].sz = g_smem_drb_qsize[i];

		pos += (sizeof(struct drb_queue_info));
	}

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		len = (g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->sz * sizeof(struct dpmaif_drb_pd)) * 2;
		if ((pos + len) > size) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: smem size too small.(%d/%u)\n",
				__func__, pos + len, size);
			return -1;
		}

		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbase[i] = (struct dpmaif_drb_pd *)
							(g_dpmaif_ctrl_v1_mt6789->smem_base_vir + pos);
		pos += len;
	}

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		len = g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->sz * DPMA_SKB_DATA_LEN;
		if ((pos + len) > size) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: smem size too small.(%d/%u)\n",
				__func__, pos + len, size);
			return -1;
		}

		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_vir[i] =
			(void *)(g_dpmaif_ctrl_v1_mt6789->smem_base_vir + pos);
		g_dpmaif_ctrl_v1_mt6789->smem_drb_qbuf_phy[i] = g_dpmaif_ctrl_v1_mt6789->smem_base_phy + pos;
		pos += len;
	}

	CCCI_NORMAL_LOG(-1, TAG, "[%s] pos: %d; size: %u)\n", __func__, pos, size);

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		len = (g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->sz * sizeof(struct dpmaif_drb_skb)) * 2;

		g_dpmaif_ctrl_v1_mt6789->dpma_drb_qskb[i] = kzalloc(len, GFP_KERNEL);
		if (g_dpmaif_ctrl_v1_mt6789->dpma_drb_qskb[i] == NULL) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kzalloc dpma_drb_qskb memory fail. len:%d\n",
				__func__, len);
			return -1;
		}
	}

	return 0;
}

static void dpmaif_smem_tx_done(struct work_struct *work)
{
	int i, ret, retry = 0;

do_retry:
	if (dpmaif_wait_resume_done()) {
		//if resume not done, will waiting 10ms
		ret = queue_delayed_work(g_dpmaif_ctrl_v1_mt6789->smem_worker,
					&g_dpmaif_ctrl_v1_mt6789->smem_drb_work,
					msecs_to_jiffies(10));
		return;
	}

	for (i = 0; i < DPMA_UL_QUEUE_NUM; i++) {
		/* This is used to avoid race condition which may cause KE */
		if (g_dpmaif_ctrl->dpmaif_state != DPMAIF_STATE_PWRON) {
			CCCI_ERROR_LOG(0, TAG, "[%s] meet hw power down.\n", __func__);
			goto do_exit;
		}

		if (!g_dpmaif_ctrl->txq[i].started) {
			CCCI_ERROR_LOG(0, TAG, "[%s] meet queue stop(%d)\n", __func__, i);
			goto do_exit;
		}

		ret = dpmaif_smem_release_tx_buffer(i);
		if (ret > 0)
			retry = 1;
		if (g_debug_flags & DEBUG_TX_START) {
			struct debug_tx_start_hdr hdr;

			hdr.type = TYPE_TX_START_ID;
			hdr.qidx = i;
			hdr.time = (unsigned int)(local_clock() >> 16);
			hdr.drb_rel = g_dpmaif_ctrl_v1_mt6789->smem_drb_qinfo[i]->rel;
			hdr.rel_cnt = ret;

			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}
	}

	if (retry) {
		retry = 0;
		goto do_retry;
	}

do_exit:
	ccif_mask_setting(ID_CCIF_USER_DATA, 0);
}

int dpmaif_tx_sw_solution_init(void)
{
	int ret;

	g_dpmaif_ctrl->tx_sw_solution = kzalloc(sizeof(struct dpmaif_ctrl_v1), GFP_KERNEL);
	if (!g_dpmaif_ctrl->tx_sw_solution) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: alloc dpmaif_ctrl_v1_6789 fail\n", __func__);
		return -1;
	}

	g_dpmaif_ctrl_v1_mt6789 = &g_dpmaif_ctrl->tx_sw_solution->tx_use_ccif_isr;
	g_dpmaif_ctrl_v1_mt6789->smem_base_vir = get_smem_start_addr(SMEM_USER_MD_DATA,
						&g_dpmaif_ctrl_v1_mt6789->smem_size);
	g_dpmaif_ctrl_v1_mt6789->smem_base_phy = ccci_get_md_view_phy_addr_by_user_id(
							SMEM_USER_MD_DATA);

	if (g_dpmaif_ctrl_v1_mt6789->smem_base_vir == NULL || g_dpmaif_ctrl_v1_mt6789->smem_base_phy == 0 ||
				g_dpmaif_ctrl_v1_mt6789->smem_size <= 0) {

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: fail. smem_base: %p(%llu); smem_size: %u\n",
			__func__, g_dpmaif_ctrl_v1_mt6789->smem_base_vir, g_dpmaif_ctrl_v1_mt6789->smem_base_phy,
			g_dpmaif_ctrl_v1_mt6789->smem_size);
		return -1;
	}

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] tx_sw_solution_enable: smem_base: %p(0x%llX); smem_size: %u\n", __func__,
		g_dpmaif_ctrl_v1_mt6789->smem_base_vir, g_dpmaif_ctrl_v1_mt6789->smem_base_phy,
		g_dpmaif_ctrl_v1_mt6789->smem_size);

	INIT_DELAYED_WORK(&g_dpmaif_ctrl_v1_mt6789->smem_drb_work, &dpmaif_smem_tx_done);
	g_dpmaif_ctrl_v1_mt6789->smem_worker = alloc_workqueue("smem_ul_worker",
					WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (g_dpmaif_ctrl_v1_mt6789->smem_worker == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] error: alloc_workqueue() fail.\n", __func__);
		return -1;
	}

	ret = register_ccif_irq_cb(ID_CCIF_USER_DATA, &dpmaif_smem_tx_irq_func);
	if (ret)
		return ret;

	ret = dpmaif_alloc_smem_to_drb();
	if (ret)
		return ret;

	ops.drv_hw_reset = &drv1_hw_reset_for_6789;
	ccmni_ops.send_skb = &dpmaif_tx_send_skb_to_smem;
	return 0;
}

void drv1_hw_reset_for_6789(void)
{
	unsigned int reg_value = 0;
	int count = 0, ret;

	udelay(500);

	/* pre- DPMAIF HW reset: bus-protect */
	ret = regmap_write(g_dpmaif_ctrl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1_SET_WA,
		DPMAIF_SLEEP_PROTECT_CTRL_WA);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d;\n",
			__func__, __LINE__, ret);
	while (1) {
		ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_TOPAXI_PROTECT_READY_STA1_1_WA, &reg_value);
		if (ret) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
				__func__, __LINE__, ret, reg_value);
			continue;
		}
		if ((reg_value & DPMAIF_SLEEP_PROTECT_CTRL_WA) == DPMAIF_SLEEP_PROTECT_CTRL_WA)
			break;

		udelay(1);
		if (++count >= 1000) {
			CCCI_ERROR_LOG(0, TAG, "DPMAIF pre-reset timeout, reg:%x\n", reg_value);
			break;
		}
	}

	ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1_WA, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	CCCI_NORMAL_LOG(0, TAG,
		"infra_topaxi_protecten_1: 0x%x\n", reg_value);

	udelay(500);
	/* reset dpmaif hw: AO Domain */
	ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_RST0_REG_AO, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_AO_RST_MASK); /* the bits in reg is WO, */
	reg_value |= (DPMAIF_AO_RST_MASK);/* so only this bit effective */

	ret = regmap_write(g_dpmaif_ctrl->infra_ao_base, INFRA_RST0_REG_AO, reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	udelay(500);

	/* reset dpmaif clr */
	ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_RST1_REG_AO, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_AO_RST_MASK);/* read no use, maybe a time delay */
	reg_value |= (DPMAIF_AO_RST_MASK);

	ret = regmap_write(g_dpmaif_ctrl->infra_ao_base, INFRA_RST1_REG_AO, reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	udelay(500);

	/* reset dpmaif hw: PD Domain */
	ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_RST0_REG_PD, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);

	ret = regmap_write(g_dpmaif_ctrl->infra_ao_base, INFRA_RST0_REG_PD, reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	udelay(500);

	/* reset dpmaif clr */
	ret = regmap_read(g_dpmaif_ctrl->infra_ao_base, INFRA_RST1_REG_PD, &reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: read infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	reg_value &= ~(DPMAIF_PD_RST_MASK);
	reg_value |= (DPMAIF_PD_RST_MASK);

	ret = regmap_write((void *)g_dpmaif_ctrl->infra_ao_base, INFRA_RST1_REG_PD, reg_value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
	udelay(500);

	/* post- DPMAIF HW reset: bus-protect */
	ret = regmap_write((void *)g_dpmaif_ctrl->infra_ao_base, INFRA_TOPAXI_PROTECTEN_1_CLR_WA,
		DPMAIF_SLEEP_PROTECT_CTRL_WA);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s]-%d error: write infra_ao_base fail; ret=%d; value: 0x%x\n",
			__func__, __LINE__, ret, reg_value);
}
