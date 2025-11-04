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

static unsigned int hid_blk_flow;
module_param(hid_blk_flow, uint, 0644);
MODULE_PARM_DESC(hid_blk_flow, "Blocking Test Select");
#define HID_BLK_GIVEBACK   (0x1U << 0)
#define HID_BLK_DSP_IRQ    (0x1U << 1)
#define HID_BLK_SUSPEND    (0x1U << 2)
#define HID_BLK_TEST(_bit) \
	(uodev->policy.hid_dbg_flow && ((hid_blk_flow & _bit) != 0))

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

enum {
	HID_ACTIVE,
	HID_DSP_RUNNING,
	HID_BUSY,
	HID_DSP_START_EVER,
	HID_DSP_IRQ_ON,
};

/* counter bit defined */
#define HID_GIVEBACK_CNT_MASK	GENMASK(3, 0)
#define HID_GIVEBACK_CNT_SHIFT	(0)
#define HID_PAYLOAD_CNT_MASK	GENMASK(7, 4)
#define HID_PAYLOAD_CNT_SHIFT	(4)
#define HID_SKIP_URB_CNT_MASK	GENMASK(11, 8)
#define HID_SKIP_URB_CNT_SHIFT	(8)

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
#define UO_HID_WAIT_RESET_NS 5000000000 /* 5 sec */

struct workqueue_struct *wq;

struct hid_ep_info {
	char name[HID_EP_NAME_LEN];
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	int dir;
	int xfer_length;
	unsigned int slot_id;
	unsigned int ep_id;
	struct uo_buffer *buf_payload;

	/* synchronization */
	unsigned long sync_flag;
	atomic_t cnt;
	spinlock_t lock;

	struct list_head skip_urb_list;
	struct list_head payload_list;
	struct delayed_work giveback;
	struct delayed_work reset;
};

static struct hid_ep_info hid_ep[UO_HID_EP_NUM];
#define get_hid_ep(dir) (dir ? &hid_ep[1] : &hid_ep[0])

/* store urb submission request from hid driver */
struct skip_urb {
	struct urb *urb;
	struct list_head list;
};

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
static void dump_urb(struct urb *urb, const char *tag);
static void giveback_hid(struct work_struct *work_struct);
static void giveback_urb(struct urb *urb, int actual_length, int status);
static struct dsp_payload *new_payload(struct hid_ep_info *hid, int length, int status);
static void free_payload(struct dsp_payload *payload);
static struct dsp_payload *fetch_payload(struct hid_ep_info *hid);
static void add_payload(struct hid_ep_info *hid, struct dsp_payload *payload);
static void remove_payload(struct hid_ep_info *hid, struct dsp_payload *payload);
static void clear_payload_list(struct hid_ep_info *ep);
static struct skip_urb *new_skip_urb(struct urb *urb);
static void free_skip_urb(struct skip_urb *skip_urb);
static struct skip_urb *fetch_skip_urb(struct hid_ep_info *hid);
static int add_skip_urb(struct hid_ep_info *hid, struct skip_urb *skip_urb);
static void remove_skip_urb(struct hid_ep_info *hid, struct skip_urb *skip_urb);
static struct skip_urb *clear_skip_urb(struct hid_ep_info *hid, struct urb *urb);
static void clear_skip_urb_list(struct hid_ep_info *hid, struct list_head *store_list);
static bool is_valid_hid_urb(struct urb *urb, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc);

/* hid ep helper */
static void hid_dump_ep(struct hid_ep_info *hid, const char *tag);
static inline void hid_ep_active(struct hid_ep_info *hid, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc, struct urb *urb);
static inline void hid_ep_reset(struct hid_ep_info *hid);
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
static void stop_dsp(struct hid_ep_info *hid, bool dsp_running);
static void hid_offload_reset(struct hid_ep_info *hid);
static bool hid_wait_idle(struct hid_ep_info *hid);

/**
 * hid_trace_dequeue - trace killed urb & init hid_ep_info
 *
 * called when hid driver kill urb, might sleep, do not hold lock here
 */
static void hid_trace_dequeue(void *unused, struct urb *urb)
{
	struct usb_interface_descriptor intf_desc;
	struct usb_endpoint_descriptor ep_desc;
	struct hid_ep_info *hid;
	struct skip_urb *skip_urb = NULL;
	bool reset = false;
	int dir;

	if (uodev->policy.hid_disable_offload)
		return;

	dir = usb_endpoint_dir_in(&urb->ep->desc);
	hid = get_hid_ep(dir);

	if (!is_valid_hid_urb(urb, &intf_desc, &ep_desc))
		return;

	if (!hid_wait_idle(hid)) {
		uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_TIMEOUT);
		hid_dump_ep(hid, "<HID Dequeue:TIMEOUT>");
		return;
	}

	hid_lock(hid, __func__);
	hid_dump_ep(hid, "<HID Dequeue:Check>");
	if (!test_bit(HID_ACTIVE, &hid->sync_flag)) {
		if (uodev->is_streaming) {
			/* scenario: HID offloading begin*/
			hid_ep_active(hid, &intf_desc, &ep_desc, urb);
			hid_dump_ep(hid, "<HID Dequeue:Active>");
		} else
			/* scenario: Suspend w/o streaming*/
			goto ignore;
	} else {
		skip_urb = clear_skip_urb(hid, urb);
		if (!get_cnt(hid, GIVEBACK)) {
			/* scenario: Abnormal case */
			reset = true;
			set_bit(HID_BUSY, &hid->sync_flag);
			hid_dump_ep(hid, "<HID Dequeue:Abnormal>");
		} else
			/* scenario: Wait second perss event */
			hid_dump_ep(hid, "<HID Dequeue:Ignore>");
	}

ignore:
	hid_unlock(hid, __func__);
	if (skip_urb) {
		/* -ENOENT make hid driver stopping enqueue*/
		giveback_urb(skip_urb->urb, 0, -ENOENT);
		free_skip_urb(skip_urb);
	}
	if (reset)
		queue_delayed_work(wq, &hid->reset, msecs_to_jiffies(0));
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
	struct skip_urb *skip_urb;
	int delay_ms = 3;
	int payload_cnt = 0;
	bool skip_enqueue = false;

	if (!is_valid_hid_urb(urb, &intf_desc, &ep_desc))
		goto ignore;

	hid = get_hid_ep_safe(usb_endpoint_dir_in(&urb->ep->desc),
			urb->dev->slot_id, xhci_get_endpoint_index_(&urb->ep->desc));
	if (unlikely(!hid))
		goto ignore;

	/* do not make decision while previous round was resetting */
	if (!hid_wait_idle(hid)) {
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
		hid_dump_ep(hid, "<HID Enqueue:TIMEOUT>");
		goto ignore;
	}

	/* we can fully trust the flags now */
	hid_lock(hid, __func__);
	if (!test_bit(HID_ACTIVE, &hid->sync_flag)) {
		/* scenario: HID offloading hans't started */
		hid_unlock(hid, __func__);
		goto ignore;
	}
	if (!test_bit(HID_DSP_START_EVER, &hid->sync_flag)) {
		/* scenario: Abnormal case */
		set_bit(HID_BUSY, &hid->sync_flag);
		hid_dump_ep(hid, "<HID Enqueue:Ignore>");
		hid_unlock(hid, __func__);
		hid_offload_reset(hid);
		goto ignore;
	}
	skip_urb = new_skip_urb(urb);
	if (!skip_urb || add_skip_urb(hid, skip_urb) < 0) {
		/* scenario: Abnormal case*/
		hid_dump_ep(hid, "<HID Enqueue:Error>");
		hid_unlock(hid, __func__);
		goto ignore;
	} else {
		/* scenario: Enqueue offloading to DSP */
		skip_enqueue = true;
		payload_cnt = get_cnt(hid, PAYLOAD);
		hid_dump_ep(hid, "<HID Enqueue>");
		hid_unlock(hid, __func__);

		if (payload_cnt > 0) {
			hid_dbg("giveback begin after %d ms\n", delay_ms);
			queue_delayed_work(wq, &hid->giveback, msecs_to_jiffies(delay_ms));
		}
	}
ignore:
	return skip_enqueue;
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
	bool lock = false;
	int delay_ms = 0;
	int ret = 0;

	hid_info("buffer:0x%llx actual_length:%d status:%d more_complete:%d\n",
		urb_complete->urb_start_addr, urb_complete->actual_length,
		urb_complete->status, urb_complete->more_complete);

	if (!hid_wait_idle(hid)) {
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
		hid_dump_ep(hid, "<DSP IRQ:TIMEOUT>");
		goto error;
	}

	hid_lock(hid, __func__);
	lock = true;

	if (!test_bit(HID_DSP_IRQ_ON, &hid->sync_flag) ||
		(unsigned long long)hid->buf_payload->phys != urb_complete->urb_start_addr) {
		hid_dump_ep(hid, "<DSP IRQ:Skip>");
		goto error;
	}
	if (!urb_complete->more_complete)
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
	if (HID_BLK_TEST(HID_BLK_DSP_IRQ)) {
		hid_info("%s Test blocking DSP ISR\n", hid->name);
		goto error;
	}
	/* create a new payload */
	payload = new_payload(hid, urb_complete->actual_length, urb_complete->status);
	if (!payload)
		goto error;
	add_payload(hid, payload);
	giveback = (get_cnt(hid, SKIP_URB) > 0);
	hid_dump_ep(hid, "<DSP IRQ:Complete>");
	hid_unlock(hid, __func__);
	lock = false;

	if (uodev->dev->power.wakeup)
		__pm_wakeup_event(uodev->dev->power.wakeup, UO_HID_WAKEUP_TIME);

	/* if there's skipped urb, giveback it */
	if (giveback) {
		hid_dbg("giveback begin after %d ms\n", delay_ms);
		queue_delayed_work(wq, &hid->giveback, msecs_to_jiffies(delay_ms));
	}
error:
	if (lock)
		hid_unlock(hid, __func__);
	return ret;
}

static void dump_urb(struct urb *urb, const char *tag)
{
	hid_dbg("%s urb:%p unlinked:%d use_count:%d reject:%d\n", tag,
		urb, urb->unlinked, atomic_read(&urb->use_count), atomic_read(&urb->reject));
}

static void giveback_urb(struct urb *urb, int actual_length, int status)
{
	struct usb_hcd *hcd = bus_to_hcd(urb->dev->bus);

	hid_info("giveback urb:%p length:%d status:%d\n", urb, actual_length, status);
	urb->actual_length = actual_length;
	usb_hcd_unlink_urb_from_ep(hcd, urb);
	usb_hcd_giveback_urb(hcd, urb, status); /* unlinked = status */
	dump_urb(urb, "<Giveback URB>");
}

static void giveback_hid(struct work_struct *work_struct)
{
	struct hid_ep_info *hid = container_of(
		work_struct, struct hid_ep_info, giveback.work);
	struct dsp_payload *payload;
	struct skip_urb *skip_urb;
	bool press_complete = false;

	if (HID_BLK_TEST(HID_BLK_GIVEBACK)) {
		hid_err("%s  Test blocking HID giveback\n", hid->name);
		return;
	}

	hid_lock(hid, __func__);

	/* fetch payload & skip_urb */
	skip_urb = fetch_skip_urb(hid);
	payload = fetch_payload(hid);
	if (!payload || !skip_urb) {
		uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);
		hid_unlock(hid, __func__);
		return;
	}

	/* update synchronization */
	remove_payload(hid, payload);
	remove_skip_urb(hid, skip_urb);
	add_cnt(hid, GIVEBACK);
	if (get_cnt(hid, GIVEBACK) == GIVEBACK_COMPLTETE) {
		hid_info("KEY-PRESS COMPLETE!!!!! Disable DSP IRQ\n");
		press_complete = true;
		clear_bit(HID_DSP_IRQ_ON, &hid->sync_flag);
		set_bit(HID_BUSY, &hid->sync_flag);
	}
	hid_dump_ep(hid, "<Giveback HID>");
	hid_unlock(hid, __func__);

	if (payload->actual_length && payload->data != NULL)
		memcpy(skip_urb->urb->transfer_buffer, payload->data, payload->actual_length);

	if (press_complete) {
		/* to prevent from hid driver submitted next urb prior to final stage,
		 * resetting was required to run before giveback
		 */
		hid_offload_reset(hid);
	}
	giveback_urb(skip_urb->urb, payload->actual_length, payload->status);
	free_payload(payload);
	free_skip_urb(skip_urb);
}

static void hid_offload_reset_work(struct work_struct *work_struct)
{
	struct hid_ep_info *hid = container_of(
		work_struct, struct hid_ep_info, reset.work);

	hid_offload_reset(hid);
}

/**
 * hid_offload_reset - final stage of 1 time hid offloading
 *
 * lock wouldn't be held in the end, but next time when hid killed urb,
 * we should monitor HID_BUSY bit to check if previous round finished
 */
static void hid_offload_reset(struct hid_ep_info *hid)
{
	u64 t_start;
	struct list_head giveback_list;
	struct skip_urb *pos, *next;
	int skip_urb_cnt = 0;
	bool dsp_running;

	t_start = ktime_get_ns();

	hid_lock(hid, __func__);
	dsp_running = (test_bit(HID_DSP_RUNNING, &hid->sync_flag));
	set_bit(HID_BUSY, &hid->sync_flag);
	hid_dump_ep(hid, "<HID RESET Start>");

	/* clear payload_list */
	if (get_cnt(hid, PAYLOAD) > 0)
		clear_payload_list(hid);

	/* clear skip_urb_list */
	INIT_LIST_HEAD(&giveback_list);
	skip_urb_cnt = get_cnt(hid, SKIP_URB);
	if (skip_urb_cnt > 0)
		clear_skip_urb_list(hid, &giveback_list);
	hid_unlock(hid, __func__);

	stop_dsp(hid, dsp_running);

	/* giveback all skip_urb here */
	if (skip_urb_cnt > 0 && !list_empty(&giveback_list)) {
		hid_info("Giveback %d ERR URB\n", skip_urb_cnt);
		list_for_each_entry_safe(pos, next, &giveback_list, list) {
			if (!usb_hcd_check_unlink_urb(bus_to_hcd(pos->urb->dev->bus), pos->urb, 0))
				giveback_urb(pos->urb, 0, -EPERM);
			list_del(&pos->list);
			free_skip_urb(pos);
		}
	}

	hid_lock(hid, __func__);
	clear_bit(HID_BUSY, &hid->sync_flag);
	hid_ep_reset(hid);
	hid_dump_ep(hid, "<HID RESET End>");
	hid_unlock(hid, __func__);

	hid_info("spend:%lld ns\n", ktime_get_ns() - t_start);
}

/**
 * hid_wait_idle - wait until driver's idle
 *
 * return false if @UO_HID_WAIT_RESET_NS elapsed, otherwise, true.
 */
static bool hid_wait_idle(struct hid_ep_info *hid)
{
	u64 t_start;
	int retval;

	hid_dbg("wait reset for %ld ns.....\n", UO_HID_WAIT_RESET_NS);

	t_start = ktime_get_ns();
	retval = wait_condition(
		(test_bit(HID_BUSY, &hid->sync_flag) == 0), UO_HID_WAIT_RESET_NS);

	if (retval < 0)
		hid_err("%s driver's still busy (spend:%lld ns)\n",
			hid->name, ktime_get_ns() - t_start);
	else
		hid_dbg("%s driver's idle (spend:%lld ns)\n",
			hid->name, ktime_get_ns() - t_start);

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
	int i, retval;

	if (uodev->policy.hid_disable_offload)
		return 0;

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		if (!hid_wait_idle(hid)) {
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
			hid_dump_ep(hid, "<Suspend:Wait Idle TIMEOUT>");
			continue;
		}
		hid_lock(hid, __func__);
		if (!test_bit(HID_ACTIVE, &hid->sync_flag) ||
			test_bit(HID_DSP_RUNNING, &hid->sync_flag)) {
			hid_unlock(hid, __func__);
			continue;
		}
		if (HID_BLK_TEST(HID_BLK_SUSPEND)) {
			hid_err("%s Test blocking Suspend\n", hid->name);
			hid_unlock(hid, __func__);
			continue;
		}
		set_bit(HID_BUSY, &hid->sync_flag);
		hid_unlock(hid, __func__);

		retval = start_dsp(hid);
		hid_lock(hid, __func__);
		if (retval < 0) {
			clear_bit(HID_ACTIVE, &hid->sync_flag);
			hid_err("fail to start dsp, HID WASN'T Offloading!!\n");
		} else {
			hid_info("success to start dsp, HID WAS Offloading!!\n");
			set_bit(HID_DSP_START_EVER, &hid->sync_flag);
		}
		set_bit(HID_DSP_IRQ_ON, &hid->sync_flag);
		clear_bit(HID_BUSY, &hid->sync_flag);
		hid_dump_ep(hid, "<Suspend>");
		hid_unlock(hid, __func__);
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
	bool reset, err;
	int i;

	if (uodev->policy.hid_disable_offload)
		return;

	/* prevent from there's any ipi message behind */
	wait_condition((false), 200000);

	for (i = 0; i < UO_HID_EP_NUM; i++) {
		hid = &hid_ep[i];
		reset = false;
		err = false;

		if (!hid_wait_idle(hid)) {
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
			hid_dump_ep(hid, "<Resume TIMEOUT>");
			continue;
		}

		hid_lock(hid, __func__);
		if (!test_bit(HID_ACTIVE, &hid->sync_flag)) {
			hid_unlock(hid, __func__);
			continue;
		}

		giveback_cnt = get_cnt(hid, GIVEBACK);
		payload_cnt = get_cnt(hid, PAYLOAD);

		if (payload_cnt > 0)
			hid_dump_ep(hid, "<Resume: Wait AP Enqueue>");
		else if (giveback_cnt > GIVEBACK_COMPLTETE) {
			hid_dump_ep(hid, "<Resume: Abnormal Count>");
			err = true;
			reset = true;
		} else if (giveback_cnt == GIVEBACK_COMPLTETE)
			hid_dump_ep(hid, "<Resume: Press Complete Before>");
		else if (giveback_cnt == 0) {
			hid_dump_ep(hid, "<Resume: User Didn't Press>");
			reset = true;
		} else
			hid_dump_ep(hid, "<Resume: Wait DSP IRQ>");

		if (reset) {
			clear_bit(HID_DSP_IRQ_ON, &hid->sync_flag);
			set_bit(HID_BUSY, &hid->sync_flag);
		}
		hid_unlock(hid, __func__);

		if (err)
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_ABNORMAL_BEHAVIOR);

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
		if (!hid_wait_idle(hid)) {
			uo_mbrain_update(UO_PHASE_HID_ONGOING, UO_ERROR_TIMEOUT);
			hid_dump_ep(hid, "<HID Stop TIMEOUT>");
		}

		hid_lock(hid, __func__);
		if (!test_bit(HID_ACTIVE, &hid->sync_flag)) {
			hid_unlock(hid, __func__);
			continue;
		}
		set_bit(HID_BUSY, &hid->sync_flag);
		clear_bit(HID_DSP_IRQ_ON, &hid->sync_flag);
		hid_unlock(hid, __func__);

		hid_offload_reset(hid);
		hid_dump_ep(hid, "<Force Stop DSP>");
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
	urb_size = (unsigned int)hid->xfer_length;
	hid->buf_payload = uob_get_empty(UO_STRUCT_URB);
	if (unlikely(!hid->buf_payload) ||
		mtk_offload_alloc_mem(hid->buf_payload, urb_size, 64,
			usb_offload_mem_type_lp(), UO_STRUCT_URB, true) < 0) {
		uo_mbrain_update(UO_PHASE_HID_START, UO_ERROR_ALLOC_URB_FAIL);
		return -ENOMEM;
	}

	/* fill message */
	memcpy(&msg.intf_desc, &hid->intf_desc, sizeof(hid->intf_desc));
	memcpy(&msg.ep_desc, &hid->ep_desc, sizeof(hid->ep_desc));
	msg.slot_id = hid->slot_id;
	msg.enable = true;
	msg.direction = (unsigned char)hid->dir;
	msg.urb_size = (unsigned int)hid->xfer_length;
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
static void stop_dsp(struct hid_ep_info *hid, bool dsp_running)
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
	if (dsp_running && !usb_offload_send_ipi_msg(UOI_DISABLE_HID, &msg, sizeof(struct usb_offload_urb_msg))) {
		clear_bit(HID_DSP_RUNNING, &hid->sync_flag);
		hid_dump_ep(hid, "<End DSP>");
	}

	/* wait for dsp */
	mdelay(10);

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
		INIT_DELAYED_WORK(&hid->reset, hid_offload_reset_work);
		INIT_LIST_HEAD(&hid->payload_list);
		INIT_LIST_HEAD(&hid->skip_urb_list);
		spin_lock_init(&hid->lock);
		cnt = snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot? ep?)",
			hid->dir);
		if (!cnt)
			hid_dbg("hid name might be weird\n");
	}

	xhci_mtk_trace_init();
}

static inline void hid_ep_active(struct hid_ep_info *hid, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc, struct urb *urb)
{
	hid->intf_desc = *intf_desc;
	hid->ep_desc = *ep_desc;
	hid->xfer_length = urb->transfer_buffer_length;
	hid->slot_id = urb->dev->slot_id;
	hid->ep_id = xhci_get_endpoint_index_(&hid->ep_desc);

	if (!snprintf(hid->name, HID_EP_NAME_LEN, "hid_ep(dir%d slot%d ep%d)", hid->dir, hid->slot_id, hid->ep_id))
		hid_dbg("hid name might be weird\n");

	set_bit(HID_ACTIVE, &hid->sync_flag);
}

static inline void hid_ep_reset(struct hid_ep_info *hid)
{
	hid->dir = 0;
	hid->slot_id = 0;
	hid->ep_id = 0;
	hid->xfer_length = 0;
	set_cnt(hid, GIVEBACK, 0);
	clear_bit(HID_ACTIVE, &hid->sync_flag);
	clear_bit(HID_DSP_START_EVER, &hid->sync_flag);
	clear_bit(HID_DSP_IRQ_ON, &hid->sync_flag);
}

static void hid_dump_ep(struct hid_ep_info *hid, const char *tag)
{
	hid_info("%s %s (need:%d run:%d busy:%d ever:%d irq:%d) (give:%lu payload:%lu skip_urb:%lu)\n",
		tag, hid->name,
		test_bit(HID_ACTIVE, &hid->sync_flag),
		test_bit(HID_DSP_RUNNING, &hid->sync_flag),
		test_bit(HID_BUSY, &hid->sync_flag),
		test_bit(HID_DSP_START_EVER, &hid->sync_flag),
		test_bit(HID_DSP_IRQ_ON, &hid->sync_flag),
		get_cnt(hid, GIVEBACK), get_cnt(hid, PAYLOAD), get_cnt(hid, SKIP_URB));
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

static struct dsp_payload *new_payload(struct hid_ep_info *hid, int length, int status)
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

	payload->status = status < 0 ? -EPROTO : 0;
	payload->actual_length = length;
	memcpy(payload->data, (void *)hid->buf_payload->virt, payload->actual_length);

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

static struct dsp_payload *fetch_payload(struct hid_ep_info *hid)
{
	return list_first_entry_or_null(&hid->payload_list, struct dsp_payload, list);
}

static void add_payload(struct hid_ep_info *hid, struct dsp_payload *payload)
{
	list_add_tail(&payload->list, &hid->payload_list);
	add_cnt(hid, PAYLOAD);
}

static void remove_payload(struct hid_ep_info *hid, struct dsp_payload *payload)
{
	list_del(&payload->list);
	sub_cnt(hid, PAYLOAD);
}

static void clear_payload_list(struct hid_ep_info *hid)
{
	struct dsp_payload *pos, *next;

	hid_info("%s clear %ld payload\n", hid->name, get_cnt(hid, PAYLOAD));
	list_for_each_entry_safe(pos, next, &hid->payload_list, list) {
		list_del(&pos->list);
		free_payload(pos);
		sub_cnt(hid, PAYLOAD);
	}
}

static struct skip_urb *new_skip_urb(struct urb *urb)
{
	struct skip_urb *skip_urb;

	skip_urb = kzalloc(sizeof(struct skip_urb), GFP_ATOMIC);
	if (skip_urb) {
		INIT_LIST_HEAD(&skip_urb->list);
		skip_urb->urb = urb;
		dump_urb(urb, "Enqueue URB");
	}
	return skip_urb;
}

static void free_skip_urb(struct skip_urb *skip_urb)
{
	kfree(skip_urb);
}

static struct skip_urb *fetch_skip_urb(struct hid_ep_info *hid)
{
	return list_first_entry_or_null(&hid->skip_urb_list, struct skip_urb, list);
}

static int add_skip_urb(struct hid_ep_info *hid, struct skip_urb *skip_urb)
{
	struct usb_hcd *hcd;
	int retval;

	if (!skip_urb)
		return -EINVAL;

	hcd = bus_to_hcd(skip_urb->urb->dev->bus);
	retval = usb_hcd_link_urb_to_ep(hcd, skip_urb->urb); /* unlinked=0 on success */
	if (!retval) {
		list_add_tail(&skip_urb->list, &hid->skip_urb_list);
		add_cnt(hid, SKIP_URB);
	} else
		hid_err("%s urb:%p can't link\n", __func__, skip_urb->urb);
	return retval;
}

static void remove_skip_urb(struct hid_ep_info *hid, struct skip_urb *skip_urb)
{
	if (!skip_urb)
		return;

	list_del(&skip_urb->list);
	sub_cnt(hid, SKIP_URB);
}

/* remove specific skip_urb from list and return it if it's necessary to giveback */
static struct skip_urb *clear_skip_urb(struct hid_ep_info *hid, struct urb *urb)
{
	struct skip_urb *pos;

	list_for_each_entry(pos, &hid->skip_urb_list, list) {
		if (pos->urb == urb) {
			remove_skip_urb(hid, pos);
			dump_urb(urb, "<Dequeue URB>");
			if (usb_hcd_check_unlink_urb(bus_to_hcd(urb->dev->bus), urb, 0) < 0) {
				hid_err("%s urb:%p has unlinked????\n", hid->name, urb);
				return NULL;
			} else
				return pos;
		}
	}
	return NULL;
}

/* remove all skip_urb from list and add those who need to giveback into store_list */
static void clear_skip_urb_list(struct hid_ep_info *hid, struct list_head *store_list)
{
	struct skip_urb *pos, *next;

	hid_info("%s clear %ld skip_urb\n", hid->name, get_cnt(hid, SKIP_URB));
	list_for_each_entry_safe(pos, next, &hid->skip_urb_list, list) {
		remove_skip_urb(hid, pos);
		dump_urb(pos->urb, "<Dequeue URB List>");
		if (usb_hcd_check_unlink_urb(bus_to_hcd(pos->urb->dev->bus), pos->urb, 0) < 0) {
			hid_err("%s urb:%p has unlinked???\n", hid->name, pos->urb);
			free_skip_urb(pos);
		} else {
			INIT_LIST_HEAD(&pos->list);
			list_add_tail(&pos->list, store_list);
		}
	}
}

static bool is_valid_hid_urb(struct urb *urb, struct usb_interface_descriptor *intf_desc,
	struct usb_endpoint_descriptor *ep_desc)
{
	struct usb_host_config *actconfig = NULL;
	struct usb_host_endpoint *ep;
	struct usb_device *dev;
	struct usb_host_interface *intf;
	struct usb_audio_dev *audio_dev;
	int intf_num, i;
	bool found_hid_req = false;

	if (!urb || !urb->dev || urb->setup_packet)
		return found_hid_req;

	dev = urb->dev;
	audio_dev = usb_offload_get_uadev(dev->slot_id);
	if (!audio_dev) {
		hid_dbg("not our audio device, slot:%d\n", dev->slot_id);
		return false;
	}

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
