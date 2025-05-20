// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Offload - HID Key Support
 * *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-chen.Liu <yu-chen.liu@mediatek.com>
 */

#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/timekeeping.h>
#include "../usb_xhci/xhci.h"
#include "../usb_xhci/xhci-mtk.h"
#include "../usb_xhci/xhci-trace.h"
#include "usb_offload.h"
#include "audio_task_usb_msg_id.h"
#include "audio_task_manager.h"

static unsigned int hid_debug;
module_param(hid_debug, uint, 0644);
MODULE_PARM_DESC(hid_debug, "Enable/Disable HID Offload debug log");

static unsigned int hid_debug_sync;
module_param(hid_debug_sync, uint, 0644);
MODULE_PARM_DESC(hid_debug_sync, "Enable/Disable HID Offload sync log");

#define hid_dbg(fmt, args...) do { \
		if (hid_debug > 0) \
			pr_info("UO, [HID] %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)
#define hid_dbg_sync(fmt, args...) do { \
		if (hid_debug_sync > 0) \
			pr_info("UO, [HID] %s(%d) " fmt, __func__, __LINE__, ## args); \
	} while (0)
#define hid_info(fmt, args...) \
	pr_info("UO, [HID] %s(%d) " fmt, __func__, __LINE__, ## args)
#define hid_err(fmt, args...) \
	pr_info("UO, [HID ERROR] %s(%d) " fmt, __func__, __LINE__, ## args)

/** synchornization bits defined
 *
 * @ NEED_OFFLOAD: if hid driver dequeued before
 * @ AP_QUEUE:     if hid driver enqueued before
 * @ ON_RESET:     if hid offloading was on resetting
 */
#define HID_NEED_OFFLOAD    0
#define HID_DSP_RUNNING     1
#define HID_AP_QUEUE        2
#define HID_ON_RESET        3

/* counter bit defined */
#define HID_GIVEBACK_CNT_MASK	GENMASK(3, 0)
#define HID_GIVEBACK_CNT_SHIFT	(0)
#define HID_PAYLOAD_CNT_MASK	GENMASK(7, 4)
#define HID_PAYLOAD_CNT_SHIFT	(4)

#define get_cnt(hid, item) \
	((atomic_read(&hid->cnt) & HID_##item##_CNT_MASK) >> HID_##item##_CNT_SHIFT)
#define add_cnt(hid, item) \
	atomic_add((0x1U << HID_##item##_CNT_SHIFT) & HID_##item##_CNT_MASK, &hid->cnt)
#define sub_cnt(hid, item) \
	atomic_sub((0x1U << HID_##item##_CNT_SHIFT) & HID_##item##_CNT_MASK, &hid->cnt)
#define set_cnt(hid, item, value) \
	atomic_set(&hid->cnt, ((value) << HID_##item##_CNT_SHIFT) & HID_##item##_CNT_MASK)

#define GIVEBACK_COMPLTETE	 2
#define HID_EP_NAME_LEN      30
#define UO_HID_EP_NUM        2
#define UO_HID_WAKEUP_TIME   1000
#define UO_HID_WAIT_RESET_NS 1000000000 /* 1 sec */

static DECLARE_COMPLETION(hid_dequeue_done);
struct workqueue_struct *wq;

struct hid_ep_info {
	char name[HID_EP_NAME_LEN];
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	struct urb *urb;
	int dir;
	unsigned int slot_id;
	unsigned int ep_id;
	struct uo_buffer *buf_payload;

	/* synchronization */
	unsigned long sync_flag;
	atomic_t cnt;
	spinlock_t lock;

	struct list_head payload_list;
	struct delayed_work giveback;
};

static struct hid_ep_info hid_ep[UO_HID_EP_NUM];
#define get_hid_ep(dir) (dir ? &hid_ep[1] : &hid_ep[0])

/* store payload info from dsp */
struct dsp_payload {
	void *data;
	int actual_length;
	int status;
	struct list_head list;
};

/* xhci helper */
static void xhci_mtk_trace_init(void);
static int xhci_realloc_hid_ring(struct hid_ep_info *hid, enum uo_provider_type id);
static struct xhci_ring *xhci_get_hid_tr_ring(struct hid_ep_info *hid);

/* urb/payload helper */
static void giveback_hid(struct work_struct *work_struct);
static void giveback_urb(struct urb *urb, int actual_length, int status);
static struct dsp_payload *new_payload(int length);
static void free_payload(struct dsp_payload *payload);
static void clear_payload_list(struct hid_ep_info *ep);
static bool is_hid_urb(struct urb *urb, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc);

/* hid ep helper */
static void hid_dump_ep(struct hid_ep_info *hid, const char *tag);
static void hid_lock(struct hid_ep_info *hid, const char *tag);
static void hid_unlock(struct hid_ep_info *hid, const char *tag);
static inline struct hid_ep_info *get_hid_ep_safe(int dir, int slot, int ep)
{
	struct hid_ep_info *hid;

	hid = get_hid_ep(dir);
	return ((hid->slot_id != slot) || (hid->ep_id != ep)) ? NULL : hid;
}

/* flow control */
static int start_dsp(struct hid_ep_info *hid);
static void stop_dsp(struct hid_ep_info *hid);
static void hid_offload_reset(struct hid_ep_info *hid);
static bool hid_wait_reset(struct hid_ep_info *hid);

/**
 * hid_trace_dequeue - trace killed urb & init hid_ep_info
 *
 * called when hid driver kill urb, might sleep, do not hold lock here
 */
static void hid_trace_dequeue(void *unused, struct urb *urb)
{
	struct hid_ep_info *hid;
	int dir, cnt;

	if (uodev->policy.hid_disable_offload || !uodev->is_streaming)
		return;

	dir = usb_endpoint_dir_in(&urb->ep->desc);
	hid = get_hid_ep(dir);

	if (is_hid_urb(urb, &hid->intf_desc, &hid->ep_desc)) {

		/* to check if previous round finished or not
		 * example: hold key would cause duration between dsp irq too long,
		 *          a dequeue event might be involved between them
		 */
		if (test_bit(HID_DSP_RUNNING, &hid->sync_flag)) {
			/* fix me: why we don't need to clear HID_AP_ENQUEUE here ?? */
			hid_dump_ep(hid, "<HID Dequeue Ignore>");
			goto ignore;
		}

		/* wait until preivious round finish */
		if (!hid_wait_reset(hid)) {
			uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_TIMEOUT);
			hid_dump_ep(hid, "<HID Dequeue ERROR>");
			return;
		}

		set_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
		hid->urb = urb;
		hid->dir = dir;
		hid->slot_id = urb->dev->slot_id;
		hid->ep_id = xhci_get_endpoint_index_(&hid->ep_desc);
		cnt = snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot%d ep%d)",
			hid->dir, hid->slot_id, hid->ep_id);
		if (!cnt)
			hid_dbg("hid name might be weird\n");
		hid_dump_ep(hid, "<HID Dequeue>");
		hid_dbg("start offloading urb %p\n", hid->urb);
	}
ignore:
	complete(&hid_dequeue_done);
}

/**
 * usb_offload_trace_hid_enqueue - determine to skip enqueue or not
 *
 * called when hid driver submit urb
 * it would giveback to hid driver if there's any stored payload before
 */
bool usb_offload_trace_hid_enqueue(struct xhci_hcd *xhci, struct urb *urb)
{
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	struct hid_ep_info *hid;
	int delay_ms = 3;

	if (!is_hid_urb(urb, &intf_desc, &ep_desc))
		return false;

	hid = get_hid_ep_safe(usb_endpoint_dir_in(&urb->ep->desc),
			urb->dev->slot_id, xhci_get_endpoint_index_(&urb->ep->desc));
	if (unlikely(!hid))
		return false;

	/* do not make decision while previous round was resetting */
	if (!hid_wait_reset(hid)) {
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
		hid_dump_ep(hid, "<AP Enqueue ERROR>");

		/* todo: shall we skip it or not ??? */
		return false;
	}

	/* we can fully trust the flags now */
	if (!test_bit(HID_NEED_OFFLOAD, &hid->sync_flag)) {
		hid_info("%s not offloading, do not skip urb %p\n", hid->name, urb);
		return false;
	}

	hid_lock(hid, __func__);
	set_bit(HID_AP_QUEUE, &hid->sync_flag);
	hid->urb = urb;
	hid_dump_ep(hid, "<AP Enqueue>");
	hid_unlock(hid, __func__);

	hid_info("skip urb %p\n", urb);

	if (get_cnt(hid, PAYLOAD) > 0) {
		hid_dbg("giveback begin after %d ms\n", delay_ms);
		queue_delayed_work(wq, &hid->giveback, msecs_to_jiffies(delay_ms));
	}

	return true;
}

/**
 * hid_dsp_irq - likes a ISR of irq from dsp (carried by ipi message)
 *
 * handle information from dsp interrupt
 * it would giveback to hid driver if there's any skipped urb before
 */
static int hid_dsp_irq(struct hid_ep_info *hid, struct usb_offload_urb_complete *urb_complete)
{
	struct dsp_payload *payload;
	bool giveback = false;
	int delay_ms = 0;
	int ret = 0;

	hid_info("buffer:0x%llx actual_length:%d status:%d more_complete:%d\n",
		urb_complete->urb_start_addr, urb_complete->actual_length,
		urb_complete->status, urb_complete->more_complete);

	/* create a new payload */
	payload = new_payload(urb_complete->actual_length);
	if (payload) {
		payload->actual_length = urb_complete->actual_length;
		if ((unsigned long long)hid->buf_payload->phys == urb_complete->urb_start_addr) {
			memcpy(payload->data, (void *)hid->buf_payload->virt, payload->actual_length);
			if (urb_complete->status < 0)
				payload->status = -EPROTO;
			else
				payload->status = 0;
		} else {
			hid_err("buffer unmatch, phy:0x%llx\n", urb_complete->urb_start_addr);
			payload->data = NULL;
			payload->status = -EPROTO;
		}
		hid_info("new payload:%p!! (length:%d status:%d)\n",
			payload, payload->actual_length, payload->status);
	} else {
		hid_err("fail allocating payload\n");
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_INSUFFICIENT_SPACE);
		goto error;
	}

	hid_lock(hid, __func__);

	hid_dump_ep(hid, "<DSP IRQ Start>");
	if (test_bit(HID_ON_RESET, &hid->sync_flag)) {
		hid_info("driver's on resetting (might be EP_STOP event), clear payload:%p\n", payload);
		goto on_resetting;
	}

	/* update synchronization */
	list_add_tail(&payload->list, &hid->payload_list);
	add_cnt(hid, PAYLOAD);
	if (!urb_complete->more_complete)
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
	giveback = (test_bit(HID_AP_QUEUE, &hid->sync_flag) != 0);

on_resetting:
	hid_dump_ep(hid, "<DSP IRQ End>");
	hid_unlock(hid, __func__);

	if (uodev->dev->power.wakeup)
		__pm_wakeup_event(uodev->dev->power.wakeup, UO_HID_WAKEUP_TIME);

	/* if there's skipped urb, giveback it */
	if (giveback) {
		hid_dbg("giveback begin after %d ms\n", delay_ms);
		queue_delayed_work(wq, &hid->giveback, msecs_to_jiffies(delay_ms));
	}
error:
	return ret;
}

static void giveback_urb(struct urb *urb, int actual_length, int status)
{
	struct usb_hcd *hcd = bus_to_hcd(urb->dev->bus);
	int ret;

	hid_info("giveback urb:%p length:%d status:%d\n", urb, actual_length, status);
	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (unlikely(ret < 0))
		hid_err("link urb error:%d\n", ret);
	urb->unlinked = 0;
	urb->actual_length = actual_length;
	usb_hcd_unlink_urb_from_ep(hcd, urb);
	usb_hcd_giveback_urb(hcd, urb, status);
}

static void giveback_hid(struct work_struct *work_struct)
{
	struct hid_ep_info *hid = container_of(
		work_struct, struct hid_ep_info, giveback.work);
	struct dsp_payload *payload;
	bool press_complete;
	struct urb *urb;

	hid_lock(hid, __func__);
	hid_dump_ep(hid, "<Start Giveback Check>");

	/* fetch first payload on list */
	if (!list_empty(&hid->payload_list)) {
		payload = list_first_entry(&hid->payload_list, struct dsp_payload, list);
		if (!payload) {
			hid_err("payload is NULL");
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);
			hid_unlock(hid, __func__);
			return;
		}
		list_del(&payload->list);
	} else {
		hid_err("payload_list is empty");
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);
		hid_unlock(hid, __func__);
		return;
	}

	/* update synchronization */
	clear_bit(HID_AP_QUEUE, &hid->sync_flag);
	sub_cnt(hid, PAYLOAD);
	add_cnt(hid, GIVEBACK);
	hid_info("giveback payload:%p!! actual_length:%d status:%d\n", payload,
		payload->actual_length, payload->status);
	hid_dump_ep(hid, "<Finish Giveback Check>");
	press_complete = (get_cnt(hid, GIVEBACK) == GIVEBACK_COMPLTETE);
	urb = hid->urb;
	hid_unlock(hid, __func__);

	if (payload->actual_length && payload->data != NULL)
		memcpy(urb->transfer_buffer, payload->data, payload->actual_length);

	if (press_complete) {
		hid_info("KEY-PRESS COMPLETE!!!!!\n");

		/* to prevent from hid driver submitted next urb prior to final stage,
		 * resetting was required to run before giveback
		 */
		hid_offload_reset(hid);
	}
	giveback_urb(urb, payload->actual_length, payload->status);
	free_payload(payload);
}

/**
 * hid_offload_reset - final stage of 1 time hid offloading
 *
 * lock wouldn't be held in the end, but next time when hid killed urb,
 * we should monitor HID_ON_RESET bit to check if previous round finished
 */
static void hid_offload_reset(struct hid_ep_info *hid)
{
	struct timespec64 start, end;
	bool giveback = false;

	ktime_get_ts64(&start);

	/* update synchorization */
	hid_lock(hid, __func__);
	set_bit(HID_ON_RESET, &hid->sync_flag);
	hid_dump_ep(hid, "<HID RESET Start>");
	if (test_bit(HID_AP_QUEUE, &hid->sync_flag)){
		clear_payload_list(hid);
		clear_bit(HID_AP_QUEUE, &hid->sync_flag);
		giveback = true;
	}
	hid_unlock(hid, __func__);

	stop_dsp(hid);

	/* giveback urb with error status if there's still skipped urb */
	if (giveback)
		giveback_urb(hid->urb, 0, -EPERM);

	hid->urb = NULL;
	hid->dir = 0;
	hid->slot_id = 0;
	hid->ep_id = 0;
	set_cnt(hid, GIVEBACK, 0);
	clear_bit(HID_ON_RESET, &hid->sync_flag);
	clear_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
	hid_dump_ep(hid, "<HID RESET End>");

	ktime_get_ts64(&end);
	hid_info("spend:%ld ns\n", end.tv_nsec - start.tv_nsec);
}

/**
 * hid_wait_reset - wait until hid_offload_reset finish
 *
 * return false if @UO_HID_WAIT_RESET_NS elapsed, otherwise, true.
 */
static bool hid_wait_reset(struct hid_ep_info *hid)
{
	struct timespec64 start, end;
	int retval;

	hid_dbg("wait reset for %d ns.....\n", UO_HID_WAIT_RESET_NS);

	ktime_get_ts64(&start);
	retval = wait_condition(
		(test_bit(HID_ON_RESET, &hid->sync_flag) == 0), UO_HID_WAIT_RESET_NS);
	ktime_get_ts64(&end);

	if (retval < 0)
		hid_err("%s timeout while waiting reset (spend:%ld ns)\n",
			hid->name, end.tv_nsec - start.tv_nsec);
	else
		hid_dbg("%s success waiting reset (spend:%ld ns)\n",
			hid->name, end.tv_nsec - start.tv_nsec);

	return !retval ? true : false;
}

/**
 * usb_offload_hid_start
 *
 * called when usb_offload.ko entered suspend state
 */
int usb_offload_hid_start(void)
{
	struct hid_ep_info *hid;
	int i;

	if (uodev->policy.hid_disable_offload)
		return 0;

	/* prevent from hid dequeue processes took too long */
	if (!wait_for_completion_timeout(&hid_dequeue_done,	msecs_to_jiffies(500))) {
		hid_err("wait dequeue timeout\n");
		return -ETIMEDOUT;
	}

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		if (test_bit(HID_NEED_OFFLOAD, &hid->sync_flag)) {

			/* to check if previous round finished or not */
			if (test_bit(HID_DSP_RUNNING, &hid->sync_flag)) {
				hid_info("dsp was still running\n");
				continue;
			}

			/* inform dsp to start hid offloading */
			if (start_dsp(hid)) {
				clear_bit(HID_NEED_OFFLOAD, &hid->sync_flag);
				hid_info("fail to start dsp, HID WASN'T Offloading!!\n");
				continue;
			} else
				hid_info("success to start dsp, HID WAS Offloading!!\n");

			hid_dump_ep(hid, "<Suspend>");
		}
	}

	return 0;
}

/**
 * usb_offload_hid_finish
 *
 * called when usb_offload.ko leaved suspend state
 */
void usb_offload_hid_finish(void)
{
	struct hid_ep_info *hid;
	unsigned long giveback_cnt, payload_cnt;
	bool reset;
	int i;

	if (uodev->policy.hid_disable_offload)
		return;

	/* prevent from there's any ipi message behind */
	wait_condition((false), 200000);

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		reset = false;

		if (!test_bit(HID_NEED_OFFLOAD, &hid->sync_flag))
			continue;

		giveback_cnt = get_cnt(hid, GIVEBACK);
		payload_cnt = get_cnt(hid, PAYLOAD);

		if (payload_cnt > 0)
			hid_dump_ep(hid, "<Resume: Wait AP Enqueue>");
		else if (giveback_cnt > GIVEBACK_COMPLTETE) {
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);
			hid_dump_ep(hid, "<Resume: Abnormal Count>");
			reset = true;
		} else if (giveback_cnt == GIVEBACK_COMPLTETE)
			hid_dump_ep(hid, "<Resume: Press Complete Before>");
		else if (giveback_cnt == 0) {
			hid_dump_ep(hid, "<Resume: User Didn't Press>");
			reset = true;
		} else
			hid_dump_ep(hid, "<Resume: Wait DSP IRQ>");

		if (reset)
			hid_offload_reset(hid);
	}
}

/**
 * usb_offload_hid_finish
 *
 * called when usb_offload.ko issued DISABLE_STREAM
 */
void usb_offload_hid_stop(void)
{
	struct hid_ep_info *hid;
	int i;

	if (uodev->policy.hid_disable_offload)
		return;

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];

		/* prevent from hid offloading was on resetting */
		if (!hid_wait_reset(hid)) {
			hid_dump_ep(hid, "<HID Stop ERROR>");
			hid_err("wait reset timeout, may cause chaos\n");
		}

		/* if dsp was still running, reset hid offloading */
		if (test_bit(HID_DSP_RUNNING, &hid->sync_flag)) {
			hid_dump_ep(&hid_ep[i], "<Force Stop DSP>");
			hid_offload_reset(hid);
		}
	}
}

/**
 * start_dsp - pre-processing stage
 *
 * inform dsp driver to start offloading hid packets and
 * handle xhci resources to be compliance to ap/dsp view
 */
static int start_dsp(struct hid_ep_info *hid)
{
	struct usb_offload_urb_msg msg = {0};
	struct xhci_ring *ring;
	struct uo_buffer *buf;
	unsigned int urb_size;

	ring = xhci_get_hid_tr_ring(hid);
	if (unlikely(!ring)) {
		hid_err("%s fail to get original ring\n", hid->name);
		return -EINVAL;
	}

	/* move trasnfer ring on ap/dsp view */
	buf = uob_search(UO_STRUCT_TRRING, ring->first_seg->dma);
	if (!buf) {
		/* put trasnfer ring on lp-seneitive memory */
		if (xhci_realloc_hid_ring(hid, usb_offload_mem_type_lp()) < 0) {
			uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_ALLOC_TR_FAIL);
			return -ENOMEM;
		}

		/* old one would be freed, get new one */
		ring = xhci_get_hid_tr_ring(hid);
		if (unlikely(!ring)) {
			uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_ALLOC_TR_FAIL);
			hid_err("%s fail to get new ring\n", hid->name);
			return -EINVAL;
		}
	}

	/* allocate data buffer on ap/dsp view */
	urb_size = (unsigned int)hid->urb->transfer_buffer_length;
	hid->buf_payload = uob_get_empty(UO_STRUCT_URB);
	if (unlikely(!hid->buf_payload) ||
		mtk_offload_alloc_mem(hid->buf_payload, urb_size, USB_OFFLOAD_TRB_SEGMENT_SIZE,
			usb_offload_mem_type_lp(), UO_STRUCT_URB, false) < 0) {
		uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_ALLOC_URB_FAIL);
		return -ENOMEM;
	}

	/* fill message */
	memcpy(&msg.intf_desc, &hid->intf_desc, sizeof(hid->intf_desc));
	memcpy(&msg.ep_desc, &hid->ep_desc, sizeof(hid->ep_desc));
	msg.slot_id = hid->slot_id;
	msg.enable = true;
	msg.direction = (unsigned char)hid->dir;
	msg.urb_size = (unsigned int)hid->urb->transfer_buffer_length;
	msg.urb_start_addr = (unsigned long long)hid->buf_payload->phys;
	msg.first_trb = (unsigned long long)ring->first_seg->dma;
	msg.cycle_state = (unsigned char)ring->cycle_state;

	/* inform dsp to start */
	if (!usb_offload_send_ipi_msg(UOI_ENABLE_HID, &msg, sizeof(struct usb_offload_urb_msg))) {
		uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_SUCCESS);
		set_bit(HID_DSP_RUNNING, &hid->sync_flag);
		return 0;
	} else {
		uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_IPI_FAIL);
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
		return -EOPNOTSUPP;
	}
}

/**
 * stop_dsp - post-processing stage
 *
 * inform dsp driver to stop offloading hid packets and
 * recycle xhci resources which we arranged before
 */
static void stop_dsp(struct hid_ep_info *hid)
{
	struct usb_offload_urb_msg msg = {
		.flag = 0,
		.slot_id = hid->slot_id,
		.enable = false,
		.direction = (unsigned char)hid->dir,
		.urb_size = 0,
		.urb_start_addr = 0,
		.first_trb = 0,
	};
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;

	/* stop endpoint first */
	if ((xhci->xhc_state & XHCI_STATE_DYING) || (xhci->xhc_state & XHCI_STATE_HALTED)) {
		hid_info("xhci was halted or dying\n");
		msg.flag |= HID_FLAG_XHCI_HALT;
	} else {
		virt_dev = uodev->xhci->devs[hid->slot_id];
		if (!virt_dev) {
			hid_err("virtual device was empty\n");
			goto skip_stop_ep;
		}

		if (xhci_stop_endpoint_sync_(xhci, &virt_dev->eps[hid->ep_id], 0, GFP_ATOMIC) < 0) {
			hid_err("fail to stop endpoint\n");
			msg.flag |= HID_FLAG_XHCI_HALT;
		} else
			hid_info("success to stop endpoint\n");
	}

skip_stop_ep:
	/* inform dsp to stop */
	if (!usb_offload_send_ipi_msg(UOI_DISABLE_HID, &msg, sizeof(struct usb_offload_urb_msg))) {
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
		hid_dump_ep(hid, "<End DSP>");
	}

	/* UO_PROV_NUM would identify as moving transfer ring back to ap view */
	xhci_realloc_hid_ring(hid, UO_PROV_NUM);

	/* release urb */
	if (mtk_offload_free_mem(hid->buf_payload))
		hid_err("fail freeing hid buf\n");
}

void usb_offload_hid_probe(void)
{
	struct hid_ep_info *hid;
	int i, cnt;

	wq = create_singlethread_workqueue("uo_hid_giveback");

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		hid->dir = i;
		INIT_DELAYED_WORK(&hid->giveback, giveback_hid);
		INIT_LIST_HEAD(&hid->payload_list);
		spin_lock_init(&hid->lock);
		cnt = snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot? ep?)",
			hid->dir);
		if (!cnt)
			hid_dbg("hid name might be weird\n");
	}

	xhci_mtk_trace_init();
}

static void hid_dump_ep(struct hid_ep_info *hid, const char *tag)
{
	hid_info("%s %s flag(need:%d dsp_run:%d queue:%d rst:%d) cnt(give:%lu payload:%lu)\n",
		tag, hid->name,
		test_bit(HID_NEED_OFFLOAD, &hid->sync_flag),
		test_bit(HID_DSP_RUNNING, &hid->sync_flag),
		test_bit(HID_AP_QUEUE, &hid->sync_flag),
		test_bit(HID_ON_RESET, &hid->sync_flag),
		get_cnt(hid, GIVEBACK), get_cnt(hid, PAYLOAD));
}

static void hid_lock(struct hid_ep_info *hid, const char *tag)
{
	hid_dbg_sync("%s wait lock\n", tag);
	spin_lock(&hid->lock);
	hid_dbg_sync("%s get lock\n", tag);
}

static void hid_unlock(struct hid_ep_info *hid, const char *tag)
{
	spin_unlock(&hid->lock);
	hid_dbg_sync("%s release lock\n", tag);
}

static int xhci_realloc_hid_ring(struct hid_ep_info *hid, enum uo_provider_type id)
{
	return xhci_mtk_realloc_transfer_ring(hid->slot_id, hid->ep_id, id, false);
}

static struct xhci_ring *xhci_get_hid_tr_ring(struct hid_ep_info *hid)
{
	struct xhci_hcd *xhci = uodev->xhci;
	struct xhci_virt_device *virt_dev;

	virt_dev = xhci->devs[hid->slot_id];
	if (unlikely(!virt_dev))
		return NULL;

	return virt_dev->eps[hid->ep_id].ring;
}

static void xhci_mtk_trace_init(void)
{
	WARN_ON(register_trace_xhci_urb_dequeue_(hid_trace_dequeue, NULL));
}

static struct dsp_payload *new_payload(int length)
{
	struct dsp_payload *payload;

	payload = kzalloc(sizeof(struct dsp_payload), GFP_ATOMIC);
	if (!payload)
		return NULL;

	payload->data = kzalloc(length, GFP_ATOMIC);
	if (!payload->data) {
		kfree(payload);
		return NULL;
	}

	INIT_LIST_HEAD(&payload->list);
	return payload;
}

static void free_payload(struct dsp_payload *payload)
{
	if (!payload)
		return;

	kfree(payload->data);
	kfree(payload);
}

static void clear_payload_list(struct hid_ep_info *hid)
{
	struct dsp_payload *pos, *next;

	list_for_each_entry_safe(pos, next, &hid->payload_list, list) {
		hid_info("clear payload:%p!!\n", pos);
		list_del(&pos->list);
		free_payload(pos);
		sub_cnt(hid, PAYLOAD);
	}
}

static bool is_hid_urb(struct urb *urb, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc)
{
	struct usb_host_config *actconfig = NULL;
	struct usb_host_endpoint *ep;
	struct usb_device *dev;
	struct usb_host_interface *intf;
	int intf_num, i;
	bool found_hid_req = false;

	if (!urb || !urb->dev || urb->setup_packet)
		return found_hid_req;

	dev = urb->dev;
	ep = urb->ep;
	actconfig = dev->actconfig;

	if (!actconfig)
		return found_hid_req;

	intf_num = actconfig->desc.bNumInterfaces;
	for (i = 0; i < intf_num; i++) {
		if (!actconfig->interface[i]->cur_altsetting)
			continue;
		intf = actconfig->interface[i]->cur_altsetting;
		if (intf->desc.bInterfaceClass != USB_CLASS_HID)
			continue;
		if (intf->endpoint == ep) {
			memcpy(intf_desc, &intf->desc, sizeof(intf->desc));
			memcpy(ep_desc, &ep->desc, sizeof(ep->desc));
			found_hid_req = true;
			break;
		}
	}
	return found_hid_req;
}

void usb_offload_ipi_hid_handler(void *param)
{
	struct ipi_msg_t *ipi_msg = NULL;
	struct hid_ep_info *hid;
	int direction, slot, ep;

	ipi_msg = (struct ipi_msg_t *)param;
	if (!ipi_msg) {
		hid_err("%s null ipi_msg\n", __func__);
		return;
	}

	hid_dbg("msg_id:0x%x\n", ipi_msg->msg_id);

	switch (ipi_msg->msg_id) {
	case AUD_USB_MSG_D2A_XHCI_IRQ:
	{
		struct usb_offload_urb_complete urb_complete;

		if (ipi_msg->payload_size != sizeof(struct usb_offload_urb_complete)) {
			hid_err("wrong payload size, msg_id:0x%x payload_size:%d\n",
				ipi_msg->msg_id, ipi_msg->payload_size);
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);
			goto error;
		} else
			memcpy(&urb_complete, (void *)ipi_msg->payload, sizeof(urb_complete));

		direction = urb_complete.direction;
		slot = urb_complete.slot_id;
		ep = urb_complete.ep_id - 1; /* minus 1 to fit ap xhci view */

		hid_dbg("[xhci irq] slot:%d ep:%d dir:%d length:%d more:%d sta:%d\n",
			slot, ep, direction, urb_complete.actual_length,
			urb_complete.more_complete,	urb_complete.status);

		hid = get_hid_ep_safe(direction, slot, ep);
		if (unlikely(!hid)) {
			hid_err("can't find hid, dir:%d slot:%d ep:%d\n", direction, slot, ep);
			break;
		}
		hid_dsp_irq(hid, &urb_complete);
		break;
	}
	default:
		break;
	}

error:
	return;
}
