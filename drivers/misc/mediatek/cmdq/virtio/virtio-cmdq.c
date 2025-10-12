// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#define pr_fmt(fmt) "virtio-cmdq: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <linux/delay.h>
#include <asm/barrier.h>
#include <linux/virtio_ring.h>
#include <linux/dma-mapping.h>
#include <linux/virtio_config.h>
#include <linux/completion.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/clk.h>
#include <linux/ratelimit.h>
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/sched/clock.h>
#include <mtk_heap.h>
MODULE_IMPORT_NS(DMA_BUF);
#endif

#include "proto.h"
#include "cmdq-util.h"
#include "virtio_ids.h"

#define CMDQ_THR_BASE			0x100
#define CMDQ_THR_SIZE			0x80
#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
#define CMDQ_MBRAIN_THRESHOLD 800000
#endif

int mtk_cmdq_log;
EXPORT_SYMBOL(mtk_cmdq_log);
module_param(mtk_cmdq_log, int, 0644);

int virtio_cmdq_latency;
EXPORT_SYMBOL(virtio_cmdq_latency);
module_param(virtio_cmdq_latency, int, 0644);

#if IS_ENABLED(CONFIG_MTK_CMDQ_DEBUG)
int virtio_cmdq_trace = 1;
#else
int virtio_cmdq_trace;
#endif
EXPORT_SYMBOL(virtio_cmdq_trace);
module_param(virtio_cmdq_trace, int, 0644);

enum {
	VIRTIO_CMDQ_VQ_REQ = 0,
	VIRTIO_CMDQ_VQ_EVT = 1,
	VIRTIO_CMDQ_VQ_MAX = 2,
};

struct virtio_cmdq {
	struct virtio_device *vdev;
	struct virtqueue *vqs[VIRTIO_CMDQ_VQ_MAX];
	spinlock_t lock;
	spinlock_t evt_lock;
	u8			hwid;
};

static struct virtio_cmdq *g_cmdq[2];
struct virtio_cmdq_req_hdr {
	bool is_event;
};

struct virtio_cmdq_event {
	struct virtio_cmdq_req_hdr hdr;
	struct cmdq_task_complete_event evt;
};

struct virtio_cmdq_req {
	struct virtio_cmdq_req_hdr hdr;
	struct cmdq_request req;
	struct cmdq_response res;
	struct completion done;
};

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
#define HEAP_SIZE (256 * 1024)
#define BUFFER_SIZE 4096
#define NUM_BUFFERS (HEAP_SIZE / BUFFER_SIZE)
static struct mutex buf_init_mutex;
static bool buf_initialed;

struct virtio_cmdq_buf {
	struct list_head	list_entry;
	bool is_used;
	void			*va_base;
	dma_addr_t		iova_base;
	dma_addr_t		pa_base;
	struct dma_heap *dma_heap;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
};
#endif

struct cmdq_service {
	struct mbox_controller mbox;
	void __iomem		*base;
	phys_addr_t		base_pa;
	uint32_t			hwid;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	uint32_t			buf_cnt;
	bool			buf_prepared;
	struct list_head	buf_list;
#endif
	struct cmdq_thread thread[CMDQ_THR_MAX_COUNT];
	spinlock_t client_pkt_list_lock;
	struct list_head client_pkt_list;

	struct clk		*clock;
	struct clk		*clock_timer;
};

static inline struct virtio_cmdq_event *to_vce(struct virtio_cmdq_req_hdr *hdr)
{
	return (struct virtio_cmdq_event *)hdr;
}

static inline struct virtio_cmdq_req *to_vcr(struct virtio_cmdq_req_hdr *hdr)
{
	return (struct virtio_cmdq_req *)hdr;
}

struct workqueue_struct *cmdq_pkt_user_err_cb;

static struct cmdq_service g_cmdq_service[2];

struct cmdq_util_platform_fp *virtio_cmdq_platform;

#define VIRTIO_TRACE_MSG_LEN	1024

static noinline int virtio_tracing_mark_write(const char *buf)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_DEBUG)
#if IS_ENABLED(CONFIG_TRACING)
	trace_puts(buf);
#endif
#endif
	return 0;
}

void virtio_cmdq_print_trace(char *fmt, ...)
{
	char buf[VIRTIO_TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len >= VIRTIO_TRACE_MSG_LEN) {
		pr_notice("%s trace size %u exceed limit\n", __func__, len);
		return;
	}
	virtio_tracing_mark_write(buf);
}
EXPORT_SYMBOL(virtio_cmdq_print_trace);

/* must need align wiht virt/grt/nebula_sched/dump.h */
#define CROSS_VM_DUMP_VIRTIO_CMDQ_G2H 4
#define CROSS_VM_DUMP_VIRTIO_CMDQ_H2G 5

struct cmdq_cross_dump_ops {
	int (*mp_set_ticks)(unsigned int cpid, unsigned int mpid);
	int (*tp_add_ticks)(unsigned int cpid, unsigned int tpid, uint64_t key);
	void (*cp_end)(unsigned int cpid);
	void (*cp_begin)(unsigned int cpid);
};
static struct cmdq_cross_dump_ops *cross_dump_ops;

void cmdq_register_cross_dump_ops(struct cmdq_cross_dump_ops *ops)
{
	cross_dump_ops = ops;
}
EXPORT_SYMBOL_GPL(cmdq_register_cross_dump_ops);

u32 *virtio_cmdq_pkt_get_perf_ret(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	if (!pkt->cmd_buf_size)
		return NULL;

	buf = list_first_entry(&pkt->buf, typeof(*buf),
		list_entry);

	return (u32 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE);
}

#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
struct workqueue_struct *cmdq_mbrain_notify;
static struct cmdq_mbrain_latency_data cmdq_mbrain;

static BLOCKING_NOTIFIER_HEAD(cmdq_mb_notifier_list);

int virtio_cmdq_mb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cmdq_mb_notifier_list, nb);
}
EXPORT_SYMBOL(virtio_cmdq_mb_register);

int virtio_cmdq_mb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cmdq_mb_notifier_list, nb);
}
EXPORT_SYMBOL(virtio_cmdq_mb_unregister);

static void virtio_cmdq_mb_event_trigger(struct work_struct *work_item)
{
	unsigned long event = CMDQ_LATENCY_TO_MB;

	blocking_notifier_call_chain(&cmdq_mb_notifier_list, event, (void *)(cmdq_mbrain.mbrain));
}

void virtio_cmdq_mb_record(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = (struct cmdq_client *)pkt->cl;
	struct cmdq_pkt_buffer *buf;
	u32 acq_time, irq_time, exec_time, wait_time, total_time = 0;
	u64 done = sched_clock();
	u32 thread_idx = virtio_cmdq_platform->get_thread_id((void *)(client->chan));
	u32 *perf = NULL;
	u32 hw_time = 0;
	unsigned long hw_time_rem = 0;
	u32 len = 0, mLen = sizeof(cmdq_mbrain.mbrain);
	static DEFINE_RATELIMIT_STATE(cmdq_mbrain_rate, 1 * HZ, 1);
	bool mbrain_trigger = false;

	perf = virtio_cmdq_pkt_get_perf_ret(pkt);
	if (perf) {
		hw_time = perf[1] > perf[0] ?
			perf[1] - perf[0] : ~perf[0] + 1 + perf[1];
		hw_time_rem= (u32)CMDQ_TICK_TO_US(hw_time);
	}

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	util_time_to_us(pkt->rec_submit, pkt->rec_trigger, acq_time);
	util_time_to_us(pkt->rec_trigger, pkt->rec_irq, irq_time);
	util_time_to_us(pkt->rec_trigger, pkt->rec_wait, wait_time);
	util_time_to_us(pkt->rec_trigger, done, exec_time);
	util_time_to_us(pkt->rec_submit, done, total_time);
	mbrain_trigger = virtio_cmdq_latency || (total_time > CMDQ_MBRAIN_THRESHOLD);

	virtio_cmdq_trace_begin("%s thd:%d pkt:%p T:%u HW:%u.%06lu hTime[%llu %llu %llu %llu]",
		__func__, thread_idx , pkt, total_time, hw_time, hw_time_rem,
		pkt->h_initTime, pkt->h_copyTime, pkt->h_handleTime, pkt->h_execTime);
	if (mbrain_trigger && __ratelimit(&cmdq_mbrain_rate)) {
		len += snprintf(cmdq_mbrain.mbrain, mLen, "thread[%u] pkt[%p] pa[%pa] iova[%pa] size[%lu] ",
			thread_idx, pkt, &buf->pa_base, &buf->iova_base, pkt->cmd_buf_size);
		len += snprintf(cmdq_mbrain.mbrain + len, mLen - len, "totalTime[%u] hwTime[%u.%06lu] ",
			total_time, hw_time, hw_time_rem);
		len += snprintf(cmdq_mbrain.mbrain + len, mLen - len, "waitTime[%u] execTime[%u] ",
			wait_time, exec_time);
		len += snprintf(cmdq_mbrain.mbrain + len, mLen - len, "acqTime[%u] irqTime[%u] ",
			acq_time, irq_time);
		len += snprintf(cmdq_mbrain.mbrain + len, mLen - len, "hostTime[%llu %llu %llu %llu]",
			pkt->h_initTime, pkt->h_copyTime, pkt->h_handleTime, pkt->h_execTime);
		if (len >= mLen)
			cmdq_err("len:%d over info size:%u", len, mLen);
		else
			cmdq_msg("%s: %s", __func__, cmdq_mbrain.mbrain);

		queue_work(cmdq_mbrain_notify, &cmdq_mbrain.cmdq_mb_notify);
	}
	virtio_cmdq_trace_end("%s thd:%d pkt:%p", __func__, thread_idx , pkt);
}
#endif

void virtio_cmdq_util_set_fp(struct cmdq_util_platform_fp *cust_cmdq_platform)
{
	if (!cust_cmdq_platform) {
		cmdq_err("%s cmdq_util_platform_fp is NULL ", __func__);
		return;
	}
	virtio_cmdq_platform = cust_cmdq_platform;
}
EXPORT_SYMBOL(virtio_cmdq_util_set_fp);

static phys_addr_t virtual_cmdq_mbox_get_base_pa(void *chan)
{
	struct cmdq_service *cmdq = container_of(((struct mbox_chan *)chan)->mbox,
		typeof(*cmdq), mbox);

	return cmdq->base_pa;
}

static u32 virtual_cmdq_util_get_hw_id(struct cmdq_pkt *pkt)
{
	uint32_t hwid;
	struct cmdq_client *client = pkt->cl;

	hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(client->chan));
	if (hwid > 2)
		cmdq_err("%s error: hwid %d is error!\n", __func__, hwid);

	return hwid;
}

void build_from_pkt(struct cmdq_pkt *pkt, struct cmdq_flush_request *req)
{
	struct cmdq_client *client = pkt->cl;
	struct cmdq_pkt_buffer *buf;
	u32 size, cnt = 0;

	list_for_each_entry (buf, &pkt->buf, list_entry) {
		if (cnt >= MAX_CMDQ_IOV_SIZE) {
			cmdq_err("[virtio] cmdq buf cnt is invalid!");
			return;
		}

		if (list_is_last(&buf->list_entry, &pkt->buf))
			size = CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
		else
			size = CMDQ_CMD_BUFFER_SIZE;

		req->iov[cnt].iov_base = buf->va_base;
		req->iov[cnt].iov_len = size;
		req->cmd_buf_paddrs[cnt] = buf->pa_base;
		req->cmd_buf_iovaddrs[cnt] = buf->iova_base;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
		req->cmd_buf_ids[cnt] = buf->buf_id;
#else
		req->cmd_buf_ids[cnt] = 0;
#endif
		cnt++;
		cmdq_log("[virtio] iova_base %pa pkt %p", &buf->iova_base, pkt);
	}

	req->key = (uint64_t)pkt;
	req->thread_id = virtio_cmdq_platform->get_thread_id((void *)(client->chan));
	req->avail_buf_size = pkt->avail_buf_size;
	req->cmd_buf_size = pkt->cmd_buf_size;
	req->buf_size = pkt->buf_size;
	req->loop = pkt->loop;
	req->priority = pkt->priority;
	req->hwid = virtual_cmdq_util_get_hw_id(pkt);
	req->iov_len = cnt;
}

static struct client_record *find_client_record(uint64_t key, uint32_t hwid)
{
	struct client_record *rec, *tmp, *found = NULL;
	unsigned long flags;
	unsigned long found_flags = 0;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	list_for_each_entry_safe (rec, tmp, &g_cmdq_service[hwid].client_pkt_list, list_entry) {
		if (rec->key == key) {
			found = rec;
			found_flags = 1;
			break;
		} else if (&rec->list_entry == (&g_cmdq_service[hwid].client_pkt_list)) {
			cmdq_msg("list done, not found");
			break;
		}
	}
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	return found;
}

static struct client_record *remove_client_record(uint64_t key, uint32_t hwid)
{
	struct client_record *rec, *tmp, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
	list_for_each_entry_safe (rec, tmp, &g_cmdq_service[hwid].client_pkt_list,
				  list_entry) {
		if (rec->key == key) {
			found = rec;
			list_del(&found->list_entry);
		}
	}
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);

	return found;
}

static void add_client_record(struct client_record *rec, uint32_t hwid)
{
	unsigned long flags;

	spin_lock_irqsave(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
	list_add_tail(&rec->list_entry, &g_cmdq_service[hwid].client_pkt_list);
	spin_unlock_irqrestore(&g_cmdq_service[hwid].client_pkt_list_lock, flags);
}

/*
static void submit_event_buffer(struct virtio_cmdq_event *vce, uint32_t hwid)
{
	struct virtqueue *vq = g_cmdq[hwid]->vqs[VIRTIO_CMDQ_VQ_EVT];
	unsigned int num_out = 0, num_in = 0;
	struct scatterlist evt_sg, *sgs[1];
	unsigned long flags;
	int ret;

	vce->hdr.is_event = true;

	spin_lock_irqsave(&g_cmdq[hwid]->lock, flags);

	sg_init_one(&evt_sg, &vce->evt, sizeof(vce->evt));
	sgs[num_out + num_in++] = &evt_sg;

	ret = virtqueue_add_sgs(vq, sgs, num_out, num_in, vce, GFP_ATOMIC);
	BUG_ON(ret != 0);

	spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);
}
*/

static void virtio_cmdq_user_err_cb(struct work_struct *work_item)
{
	struct client_record *rec =
		container_of(work_item, struct client_record, err_cb_work);
	struct cmdq_cb_data cb_data;
	struct cmdq_pkt *pkt;

	pkt = rec->pkt;
	if (pkt->err_cb.cb && pkt->err_cb.data) {
		cb_data.data = pkt->err_cb.data;
		cb_data.err = rec->host_result;
		if (!rec->user_err_dump) {
			pkt->err_cb.cb(cb_data);
			rec->user_err_dump = true;
		} else
			cmdq_msg("user may not register err cb %p or already dumped flag %d!",
				pkt->err_cb.cb, rec->user_err_dump);
	}
	cb_data.err = rec->host_result;
	cb_data.data = pkt->cb.data;
	pkt->cb.cb(cb_data);
}

static void handle_cmdq_event(struct cmdq_task_complete_event *evt, uint32_t hwid)
{
	struct client_record *rec = find_client_record(evt->key, hwid);
	struct cmdq_cb_data data;
	struct cmdq_pkt *pkt;
	struct cmdq_client *client;
	s32 thd;

	if (cross_dump_ops && cross_dump_ops->cp_end)
		cross_dump_ops->cp_end(CROSS_VM_DUMP_VIRTIO_CMDQ_H2G);

	if (rec == NULL || rec->pkt == NULL) {
		pr_err("%s %d error:no client record.\n", __func__, __LINE__);
		return;
	}
	pkt = rec->pkt;
	pkt->rec_irq = sched_clock();
	rec->host_result = evt->result;
	data.err = evt->result;
	data.data = rec->data;
	pkt->h_execTime = evt->exec_time;

	if (rec->host_result){
		client = (struct cmdq_client *)(pkt->cl);
		thd = virtio_cmdq_platform->get_thread_id((void *)(client->chan));

		cmdq_err("------ VIRTIO CMDQ BEGIN OF ERROR DUMP------");
		cmdq_err("task info thread[%d] result[%d] pkt[%p] wfe[%d] event[0x%x %u] offset[%lu]",
			thd, rec->host_result, pkt, evt->wfe, evt->event, evt->event, evt->off);
		pkt->err_data.wfe_timeout = evt->wfe;
		pkt->err_data.event = evt->event;
		pkt->err_data.offset = evt->off;

		if (pkt->cb.cb && pkt->cb.data) {
			INIT_WORK(&rec->err_cb_work, virtio_cmdq_user_err_cb);
			queue_work(cmdq_pkt_user_err_cb, &rec->err_cb_work);
		} else
			cmdq_msg("user may not register cb %p!", pkt->err_cb.cb);
	} else {
#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
		virtio_cmdq_mb_record(pkt);
#endif
		complete(&rec->pkt->cmplt);
		if (rec->cb)
			rec->cb(data);
	}
}

/* Called from virtio device, in IRQ context */
void cmdq_request_done(struct virtqueue *vq)
{
	struct virtio_cmdq *cmdq = vq->vdev->priv;
	struct virtio_cmdq_req_hdr *hdr;
	unsigned long flags;
	unsigned int len;
	bool notify = false;

	spin_lock_irqsave(&cmdq->lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_cmdq_req *vcr = to_vcr(hdr);
			complete(&vcr->done);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&cmdq->lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

static int virtio_cmdq_queue_evt_buffer(struct virtqueue *vq,
					struct virtio_cmdq_event *evt)
{
	int ret;
	struct scatterlist sg;

	sg_init_one(&sg, &evt->evt, sizeof(evt->evt));

	ret = virtqueue_add_inbuf(vq, &sg, 1, evt, GFP_ATOMIC);

	if (ret)
		return ret;

	virtqueue_kick(vq);

	return 0;
}

struct virtqueue *cmd_evtq;
void cmdq_evt_done(struct virtqueue *vq)
{
	struct virtio_cmdq *cmdq = vq->vdev->priv;
	struct virtio_cmdq_req_hdr *hdr;
	unsigned long flags;
	unsigned int len;
	bool notify = false;

	cmd_evtq = vq;
	spin_lock_irqsave(&cmdq->evt_lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_cmdq_event *vce = to_vce(hdr);
			handle_cmdq_event(&vce->evt, cmdq->hwid);
			virtio_cmdq_queue_evt_buffer(vq, vce);
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;

	spin_unlock_irqrestore(&cmdq->evt_lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

int fill_cmdq_evt(struct virtio_cmdq *cmdq)
{
	int i;
	struct virtio_cmdq_event *evts;
	struct virtqueue *vq = cmdq->vqs[VIRTIO_CMDQ_VQ_EVT];
	size_t nr_evts = vq->num_free;
	int ret;

	evts = kcalloc(nr_evts, sizeof(*evts), GFP_KERNEL);
	if (!evts)
		return -ENOMEM;

	for (i = 0; i < nr_evts; i++) {
		ret = virtio_cmdq_queue_evt_buffer(vq, &evts[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static int submit_cmdq_request(struct virtqueue *vq,
			       struct virtio_cmdq_req *vcr)
{
	struct scatterlist req_sg, res_sg, buf_sg[MAX_CMDQ_IOV_SIZE],
		*sgs[MAX_CMDQ_IOV_SIZE + 2];
	unsigned int num_out = 0, num_in = 0;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	struct scatterlist cmd_buf_sg;
#endif

	sg_init_one(&req_sg, &vcr->req, sizeof(vcr->req));
	sgs[num_out++] = &req_sg;

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	if (vcr->req.type == CMDQ_OPS_ALLOC_BUF) {
		sg_init_one(&cmd_buf_sg, vcr->req.buf_req.data_buf, vcr->req.buf_req.data_size);
		sgs[num_out++] = &cmd_buf_sg;
	}
#endif
	if (vcr->req.type == CMDQ_OPS_FLUSH) {
		struct cmdq_flush_request *flush_req = &vcr->req.flush_req;
		int i;

		for (i = 0; i < flush_req->iov_len; i++) {
			struct iovec *iov = &flush_req->iov[i];

			sg_init_one(&buf_sg[i],
				    phys_to_virt(flush_req->cmd_buf_paddrs[i]),
				    iov->iov_len);
			sgs[num_out++] = &buf_sg[i];
		}
	}

	sg_init_one(&res_sg, &vcr->res, sizeof(vcr->res));
	sgs[num_out + num_in++] = &res_sg;

	return virtqueue_add_sgs(vq, sgs, num_out, num_in, vcr, GFP_ATOMIC);
}

static int submit_req_and_wait_result(struct virtio_cmdq_req *vcr, uint32_t hwid)
{
	unsigned long flags;
	bool notify = false;
	int err;
	struct virtqueue *vq = g_cmdq[hwid]->vqs[VIRTIO_CMDQ_VQ_REQ];

	if (g_cmdq[hwid] == NULL || vcr == NULL) {
		cmdq_err("[virtio] g_cmdq[%d] or vcr is NULL!", hwid);
		return -EINVAL;
	}

	if (cross_dump_ops && cross_dump_ops->cp_begin)
		cross_dump_ops->cp_begin(CROSS_VM_DUMP_VIRTIO_CMDQ_G2H);

	spin_lock_irqsave(&g_cmdq[hwid]->lock, flags);
	err = submit_cmdq_request(vq, vcr);
	if (err) {
		spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);
		err = -ENOMEM;
		goto out;
	}

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(&g_cmdq[hwid]->lock, flags);

	if (notify)
		virtqueue_notify(vq);

	wait_for_completion(&vcr->done);
	err = vcr->res.ret;
out:
	return err;
}

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
void *virtio_cmdq_pkt_aquire_buf(dma_addr_t *iova, dma_addr_t *pa)
{
	struct virtio_cmdq_buf *tmp = NULL, *buf;
	void *va = NULL;

	mutex_lock(&buf_init_mutex);
	list_for_each_entry_safe(buf, tmp, &g_cmdq_service[0].buf_list, list_entry) {
		if (!buf->is_used) {
			buf->is_used = true;
			*iova = buf->iova_base;
			*pa = buf->pa_base;
			va = buf->va_base;
			tmp = buf;
			break;
		}
	}
	mutex_unlock(&buf_init_mutex);

	if (!va)
		cmdq_err("[vcmdq FE] %s no free buf", __func__);
	else
		cmdq_log("[vcmdq FE] %s get buf iova %pa pa %pa", __func__, &tmp->iova_base, &tmp->iova_base);

	return va;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_aquire_buf);

void virtio_cmdq_pkt_release_buf(dma_addr_t iova)
{
	struct virtio_cmdq_buf *tmp = NULL, *buf;

	mutex_lock(&buf_init_mutex);
	list_for_each_entry_safe(buf, tmp, &g_cmdq_service[0].buf_list, list_entry) {
		if (buf->iova_base == iova) {
			buf->is_used = false;
			tmp = buf;
			break;
		}
	}
	if (!tmp)
		cmdq_err("[vcmdq FE] %s free buf failed, no record", __func__);
	else
		cmdq_log("[vcmdq FE] %s put buf iova %pa pa %pa", __func__, &tmp->iova_base, &tmp->iova_base);
	mutex_unlock(&buf_init_mutex);
}
EXPORT_SYMBOL(virtio_cmdq_pkt_release_buf);

u64 virtio_cmdq_pkt_alloc_buf(struct virtio_cmdq_mem_entry *ents, u32 length, u64 *iova, u64 *pa)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct timespec64 ts;
	u64 random_number;
	u64 ret = 0;

	ktime_get_real_ts64(&ts);
	random_number = ((u64)ts.tv_sec << 32) | (u64)ts.tv_nsec;

	if (vcr == NULL || !ents) {
		cmdq_err("[vcmdq FE] %s vcr alloc failed!", __func__);
		return -EINVAL;
	}
	virtio_cmdq_trace_begin("%s key 0x%llx", __func__, random_number);
	vcr->req.buf_req.data_buf = ents;
	vcr->req.buf_req.data_size = length * sizeof(*ents);
	vcr->req.buf_req.key = random_number;
	vcr->req.buf_req.hwid = 0;
	vcr->req.type = CMDQ_OPS_ALLOC_BUF;
	init_completion(&vcr->done);

	ret = submit_req_and_wait_result(vcr, 0);
	if(ret < 0)
		cmdq_err("[vcmdq FE] %s get iova failed", __func__);
	else {
		*iova = vcr->res.iova;
		*pa = vcr->res.pa;
		ret = random_number;
	}
	cmdq_log("[vcmdq FE] %s get iova 0x%llx pa %llx, key %llu", __func__, *iova, *pa, ret);
	virtio_cmdq_trace_end();

	kfree(vcr->req.buf_req.data_buf);
	kfree(vcr);

	return ret;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_alloc_buf);
#endif

s32 virtio_cmdq_pkt_flush_async(struct cmdq_pkt *pkt, cmdq_async_flush_cb cb,
				void *data)
{
	struct virtio_cmdq_req *vcr = NULL;
	struct client_record *rec = NULL;
	int ret;
	uint32_t hwid;
	struct cmdq_client *client;
	s32 thd;
	u64 rec_submit = 0;
	u64 begin = sched_clock();
	u64 timestamp[5] = {0};
	u64 total_time = 0, vcr_time = 0, fd_list_time = 0;
	u64 submit_time = 0, build_pkt_time = 0, add_list_time = 0;

	if (pkt)
		rec_submit = pkt->rec_submit;
	else {
		cmdq_err("[virtio] %s pkt is NULL!", __func__);
		return -EINVAL;
	}

	vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	if (!vcr)
		return -EINVAL;

	timestamp[0] = sched_clock();
	ret = virtio_cmdq_platform->check_pkt_finalize((void *)pkt);
	if (ret < 0) {
		kfree(vcr);
		return ret;
	}

	hwid = virtual_cmdq_util_get_hw_id(pkt);
	rec = find_client_record((uint64_t)pkt, hwid);
	timestamp[1] = sched_clock();
	if (!rec) {
		rec = kzalloc(sizeof(*rec), GFP_KERNEL);
		if (!rec) {
			kfree(vcr);
			return -ENOMEM;
		}

		rec->key = (uint64_t)pkt;
		rec->pkt = pkt;
		add_client_record(rec, hwid);
	}
	timestamp[2] = sched_clock();

	if (rec->pkt)
		rec->pkt->rec_trigger = sched_clock();
	rec->cb = cb;
	rec->data = data;
	rec->host_result = 0;
	rec->user_err_dump = false;
	client = (struct cmdq_client *)(pkt->cl);
	thd = virtio_cmdq_platform->get_thread_id((void *)(client->chan));

	vcr->req.type = CMDQ_OPS_FLUSH;
	build_from_pkt(pkt, &vcr->req.flush_req);
	init_completion(&vcr->done);
	pkt->task_alloc = true;
	timestamp[3] = sched_clock();

	ret = submit_req_and_wait_result(vcr, hwid);
	if (ret < 0) {
		cmdq_err("%s thread %d task submit failed ret %d pkt %p",
			__func__, thd, ret, pkt);
		pkt->task_alloc = false;
	}
	timestamp[4] = sched_clock();
	util_time_to_us(begin, timestamp[0], vcr_time);
	util_time_to_us(timestamp[0], timestamp[1], fd_list_time);
	util_time_to_us(timestamp[1], timestamp[2], add_list_time);
	util_time_to_us(timestamp[2], timestamp[3], build_pkt_time);
	util_time_to_us(timestamp[3], timestamp[4], submit_time);
	util_time_to_us(rec_submit, timestamp[4], total_time);
	if (total_time > 500000 || virtio_cmdq_latency)
		cmdq_msg("%s time[%llu us! %llu, %llu, %llu, %llu, %llu](us) host_time[%llu %llu %llu](us)!\n",
			__func__, total_time, vcr_time, fd_list_time, add_list_time, build_pkt_time, submit_time,
			vcr->res.init_time, vcr->res.copy_time, vcr->res.handle_time);

	virtio_cmdq_trace_begin("%s thd[%d] time[%llu us! %llu, %llu, %llu, %llu, %llu host %llu %llu %llu](us)",
			__func__, thd, total_time, vcr_time, fd_list_time, add_list_time, build_pkt_time, submit_time,
			vcr->res.init_time, vcr->res.copy_time, vcr->res.handle_time);
	if (rec->pkt) {
		rec->pkt->rec_wait = sched_clock();
		rec->pkt->h_initTime = vcr->res.init_time;
		rec->pkt->h_copyTime = vcr->res.copy_time;
		rec->pkt->h_handleTime = vcr->res.handle_time;
	}
	kfree(vcr);
	virtio_cmdq_trace_end("thd:%d pkt:%p", thd , pkt);

	return ret;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_flush_async);

int virtio_cmdq_pkt_wait_complete(struct cmdq_pkt *pkt)
{
	struct client_record *rec;
	uint32_t hwid;

	hwid = virtual_cmdq_util_get_hw_id(pkt);
	rec = find_client_record((uint64_t)pkt, hwid);
	if (rec == NULL) {
		cmdq_err("[virtio] can't find rec from pkt");
		return -EINVAL;
	}

	wait_for_completion(&pkt->cmplt);

	return rec->host_result;
}
EXPORT_SYMBOL(virtio_cmdq_pkt_wait_complete);

void virtio_cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	struct client_record *rec;
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	uint32_t hwid = virtual_cmdq_util_get_hw_id(pkt);

	if (vcr == NULL) {
		cmdq_err("[virtio] %s vcr alloc failed!", __func__);
		return;
	}
	vcr->req.destroy_req.key = (uint64_t)pkt;
	vcr->req.destroy_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_DESTROY;
	init_completion(&vcr->done);
	hwid = virtual_cmdq_util_get_hw_id(pkt);

	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);

	rec = remove_client_record((uint64_t)pkt, hwid);
	if (!rec)
		cmdq_msg("%s can't find pkt %p", __func__, pkt);
	kfree(rec);
}
EXPORT_SYMBOL(virtio_cmdq_pkt_destroy);

void virtio_cmdq_mbox_channel_stop(struct mbox_chan *chan)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct cmdq_thread *thread = chan->con_priv;
	uint32_t hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(chan));

	if (vcr == NULL) {
		cmdq_err("[virtio] %s vcr alloc failed!", __func__);
		return;
	}
	vcr->req.chan_stop_req.thread_id = thread->idx;
	vcr->req.chan_stop_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_CHAN_STOP;
	init_completion(&vcr->done);
	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);
}
EXPORT_SYMBOL(virtio_cmdq_mbox_channel_stop);

void virtio_cmdq_mbox_enable(void *chan)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	uint32_t hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(chan));

	if (vcr == NULL) {
		cmdq_err("[virtio] %s vcr alloc failed!", __func__);
		return;
	}
	vcr->req.mbox_enable_req.thread_id = thread->idx;
	vcr->req.mbox_enable_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_MBOX_ENABLE;
	init_completion(&vcr->done);

	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);
}
EXPORT_SYMBOL(virtio_cmdq_mbox_enable);

void virtio_cmdq_mbox_disable(void *chan)
{
	struct virtio_cmdq_req *vcr = kzalloc(sizeof(*vcr), GFP_KERNEL);
	struct cmdq_thread *thread = ((struct mbox_chan *)chan)->con_priv;
	uint32_t hwid = virtio_cmdq_platform->util_hw_id((u32)virtual_cmdq_mbox_get_base_pa(chan));

	if (vcr == NULL) {
		cmdq_err("[virtio] %s vcr alloc failed!", __func__);
		return;
	}
	vcr->req.mbox_disable_req.thread_id = thread->idx;
	vcr->req.mbox_disable_req.hwid = hwid;
	vcr->req.type = CMDQ_OPS_MBOX_DISABLE;
	init_completion(&vcr->done);

	submit_req_and_wait_result(vcr, hwid);
	kfree(vcr);

}
EXPORT_SYMBOL(virtio_cmdq_mbox_disable);

static int cmdq_mbox_send_data(struct mbox_chan *chan, void *data)
{
	WARN_ON_ONCE();
	return 0;
}

static int cmdq_mbox_startup(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = true;
	return 0;
}

static void cmdq_mbox_shutdown(struct mbox_chan *chan)
{
	struct cmdq_thread *thread = chan->con_priv;

	thread->occupied = false;
}

static bool cmdq_mbox_last_tx_done(struct mbox_chan *chan)
{
	return true;
}

static void cmdq_config_dma_mask(struct device *dev)
{
	u32 dma_mask_bit = 0;
	s32 ret;

	ret = of_property_read_u32(dev->of_node, "dma-mask-bit",
		&dma_mask_bit);
	/* if not assign from dts, give default 32bit for legacy chip */
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = 32;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(dma_mask_bit));
#else
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(dma_mask_bit));
#endif
	if (ret)
		cmdq_err("virtio mbox set dma mask bit:%u result:%d\n",
			dma_mask_bit, ret);
}

static const struct mbox_chan_ops cmdq_mbox_chan_ops = {
	.send_data = cmdq_mbox_send_data,
	.startup = cmdq_mbox_startup,
	.shutdown = cmdq_mbox_shutdown,
	.last_tx_done = cmdq_mbox_last_tx_done,
};

static struct mbox_chan *cmdq_xlate(struct mbox_controller *mbox,
				    const struct of_phandle_args *sp)
{
	int ind = sp->args[0];
	struct cmdq_thread *thread;

	if (ind >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	thread = mbox->chans[ind].con_priv;
	thread->timeout_ms =
		sp->args[1] != 0 ? sp->args[1] : CMDQ_TIMEOUT_DEFAULT;
	thread->priority = sp->args[2];
	thread->chan = &mbox->chans[ind];

	return &mbox->chans[ind];
}

int virtio_cmdq_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int error;
	int i;
	static uint32_t hwid;
	struct cmdq_service *cmdq = NULL;

	cmdq = devm_kzalloc(dev, sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	spin_lock_init(&cmdq->client_pkt_list_lock);

	cmdq->mbox.dev = dev;
	cmdq->mbox.chans = devm_kcalloc(dev, CMDQ_THR_MAX_COUNT,
					sizeof(*cmdq->mbox.chans), GFP_KERNEL);
	if (!cmdq->mbox.chans) {
		error = -ENOMEM;
		goto err_out;
	}

	cmdq->mbox.num_chans = CMDQ_THR_MAX_COUNT;
	cmdq->mbox.ops = &cmdq_mbox_chan_ops;
	cmdq->mbox.of_xlate = cmdq_xlate;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		cmdq_err("failed to get resource");
		return -EINVAL;
	}
	cmdq->base = devm_ioremap_resource(dev, res);
	cmdq->base_pa = res->start;
	/* make use of TXDONE_BY_ACK */
	cmdq->mbox.txdone_irq = false;
	cmdq->mbox.txdone_poll = false;
	for (i = 0; i < ARRAY_SIZE(cmdq->thread); i++) {
		INIT_LIST_HEAD(&cmdq->thread[i].task_busy_list);
		cmdq->thread[i].base = cmdq->base + CMDQ_THR_BASE +
				CMDQ_THR_SIZE * i;
		cmdq->thread[i].gce_pa = cmdq->base_pa;
		cmdq->thread[i].idx = i;
		cmdq->thread[i].is_virtio = true;
		cmdq->mbox.chans[i].con_priv = &cmdq->thread[i];
	}

	error = mbox_controller_register(&cmdq->mbox);
	if (error < 0)
		goto err_free_chan;

	cmdq_config_dma_mask(dev);

	cmdq_pkt_user_err_cb = create_singlethread_workqueue(
			"cmdq_pkt_user_err_cb");
#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
	cmdq_mbrain_notify = create_singlethread_workqueue(
			"cmdq_mbrain_notify");
	INIT_WORK(&cmdq_mbrain.cmdq_mb_notify, virtio_cmdq_mb_event_trigger);
#endif

	g_cmdq_service[hwid] = *cmdq;
	INIT_LIST_HEAD(&g_cmdq_service[hwid].client_pkt_list);
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	INIT_LIST_HEAD(&g_cmdq_service[hwid].buf_list);
#endif
	cmdq->hwid = hwid++;
	return 0;

err_free_chan:
	kfree(cmdq->mbox.chans);
err_out:
	return error;
}

static const struct of_device_id dt_match[] = {
	{
		.compatible = "grt,virtio-cmdq",
	},
	{ /* sentinel */ },
};

static struct platform_driver virtio_cmdq_mbox_driver = {
	.probe = virtio_cmdq_mbox_probe,
	.driver = {
		.name   = "virtio_cmdq",
		.of_match_table = dt_match,
	},
};

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
static int virtio_cmdq_init_buffer_pool(struct device *dev,
	struct virtio_cmdq_buf **buf)
{
	void *va_base = NULL;
	struct scatterlist *sg = NULL;
	int i = 0;
	u64 ret = 0;
	dma_addr_t pa = 0;
	dma_addr_t iova = 0;
	struct iosys_map map = {0};
	struct dma_heap *dma_heap = NULL;
	struct dma_buf *dma_buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;
	struct virtio_cmdq_mem_entry *ents = NULL;

	dma_heap = dma_heap_find("mtk_mm-uncached");
	if(!dma_heap) {
		cmdq_err("%s get mm heap failed", __func__);
		return (-ENOMEM);
	}

	dma_buf = dma_heap_buffer_alloc(dma_heap, CMDQ_BUF_ALLOC_SIZE,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if(IS_ERR_OR_NULL(dma_buf)) {
		cmdq_err("%s alloc dma buf failed", __func__);
		dma_heap_put(dma_heap);
		return (-ENOMEM);
	}

	attach = dma_buf_attach(dma_buf, dev);
	if(IS_ERR_OR_NULL(attach)) {
		cmdq_err("%s get attach failed", __func__);
		dma_heap_buffer_free(dma_buf);
		dma_heap_put(dma_heap);
		return (-ENOMEM);
	}

	ret = dma_buf_vmap(dma_buf, &map);
	if (ret) {
		cmdq_err("%s vmap faile", __func__);
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dma_buf, attach);
		dma_heap_buffer_free(dma_buf);
		dma_heap_put(dma_heap);
		return (-ENOMEM);
	}
	va_base = map.vaddr;

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if(IS_ERR_OR_NULL(sgt)) {
		cmdq_err("%s get attach failed", __func__);
		dma_buf_detach(dma_buf, attach);
		dma_heap_buffer_free(dma_buf);
		dma_heap_put(dma_heap);
		return (-ENOMEM);
	}

	ents = kcalloc((sgt)->nents, sizeof(ents), GFP_KERNEL);
	if (!ents) {
		dma_buf_vunmap(dma_buf, &map);
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dma_buf, attach);
		dma_heap_buffer_free(dma_buf);
		dma_heap_put(dma_heap);
		return (-ENOMEM);
	}
	dsb(sy);

	for_each_sg((sgt)->sgl, sg, (sgt)->nents, i) {
		ents[i].addr = cpu_to_le64(sg_phys(sg));
		ents[i].length = cpu_to_le32(sg->length);
	}


	ret = virtio_cmdq_pkt_alloc_buf(ents, (sgt)->nents, &iova, &pa);
	if (ret < 0) {
		cmdq_err("%s get iova fail", __func__);
		dma_buf_vunmap(dma_buf, &map);
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dma_buf, attach);
		dma_heap_buffer_free(dma_buf);
		dma_heap_put(dma_heap);
		kfree(ents);
		return (-ENOMEM);
	}

	(*buf)->dma_heap = dma_heap;
	(*buf)->dma_buf = dma_buf;
	(*buf)->attach = attach;
	(*buf)->sgt = sgt;
	(*buf)->pa_base = pa;
	(*buf)->iova_base = iova;
	(*buf)->va_base = va_base;
	(*buf)->is_used = false;

	return ret;
}

static int virtio_cmdq_init_kthread(void *data)
{
	struct virtio_cmdq_buf *buf[NUM_BUFFERS] = {0};
	int ret = 0, i = 0;

	cmdq_log("%s initialing", __func__);
	mutex_lock(&buf_init_mutex);
	for (i = 0; i < NUM_BUFFERS; i++) {
		buf[i] = kzalloc(sizeof(*buf[0]), GFP_KERNEL);
		if (!buf[i])
			return -ENOMEM;

		ret = virtio_cmdq_init_buffer_pool(g_cmdq_service[0].mbox.dev, &buf[i]);
		if (ret < 0)
			cmdq_err("%s init buffer pool failed %d", __func__, ret);

		list_add_tail(&buf[i]->list_entry, &g_cmdq_service[0].buf_list);
		cmdq_log("[vcmdq FE] %s get iova %pa pa %pa, va %p", __func__,
			&buf[i]->iova_base, &buf[i]->pa_base, buf[i]->va_base);
	}
	mutex_unlock(&buf_init_mutex);
	if (i == NUM_BUFFERS) {
		g_cmdq_service[0].buf_prepared = true;
		g_cmdq_service[0].buf_cnt = NUM_BUFFERS;
	} else {
		cmdq_msg("[vcmdq FE] %s prepare buf cnt %u", __func__, i);
		g_cmdq_service[0].buf_prepared = true;
		g_cmdq_service[0].buf_cnt = i;
	}

	return ret;
}
#endif

static int virtio_cmdq_probe(struct virtio_device *vdev)
{
	vq_callback_t *vq_cbs[VIRTIO_CMDQ_VQ_MAX] = { cmdq_request_done,
						      cmdq_evt_done };
	const char *names[VIRTIO_CMDQ_VQ_MAX] = { "request", "event" };
	struct virtio_cmdq *cmdq;
	int err = -EINVAL;
	static u8 hwid;

	cmdq = kzalloc(sizeof(*cmdq), GFP_KERNEL);
	if (!cmdq)
		return -ENOMEM;

	cmdq->vdev = vdev;
	spin_lock_init(&cmdq->lock);
	spin_lock_init(&cmdq->evt_lock);

	err = virtio_find_vqs(vdev, VIRTIO_CMDQ_VQ_MAX, cmdq->vqs, vq_cbs, names,
			      NULL);
	if (err) {
		cmdq_err("virtio_find_vqs failed\n");
		goto err;
	}

	virtio_device_ready(vdev);

	err = fill_cmdq_evt(cmdq);
	if (err) {
		cmdq_err("fill_cmdq_evt failed\n");
		goto err;
	}

	vdev->priv = cmdq;
	g_cmdq[hwid] = cmdq;
	cmdq->hwid = hwid++;
	cmdq_msg("virtio cmdq probe done hwid %d size: req %lu virtq %lu sgs %lu\n",
		hwid, sizeof(struct virtio_cmdq_req), sizeof(struct virtqueue), sizeof(struct scatterlist));

	return 0;
err:
	dev_warn(&vdev->dev, "virtio cmdq probe failed:%d\n", err);

	if (cmdq->vdev)
		vdev->config->del_vqs(cmdq->vdev);
	kfree(cmdq);
	return err;
}

static void virtio_cmdq_remove(struct virtio_device *vdev)
{
	struct virtio_cmdq *cmdq = vdev->priv;

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(cmdq->vdev);
	vdev->config->del_vqs(cmdq->vdev);
	kfree(cmdq);
	g_cmdq[0] = NULL;
	g_cmdq[1] = NULL;
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_CMDQ, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {};

static struct virtio_driver virtio_cmdq_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_cmdq_probe,
	.remove = virtio_cmdq_remove,
};

static int virtio_cmdq_init(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	struct task_struct *virtio_cmdq_init_task = NULL;
#endif

	ret = platform_driver_register(&virtio_cmdq_mbox_driver);
	if (ret != 0) {
		cmdq_err("[virtio] platform register failed!");
		return ret;
	}
	ret = register_virtio_driver(&virtio_cmdq_driver);
	if (ret != 0) {
		cmdq_err("[virtio] virtio register failed!");
		return ret;
	}

#if IS_ENABLED(CONFIG_VIRTIO_CMDQ_DMA_MAP)
	mutex_init(&buf_init_mutex);
	if (!buf_initialed) {
		virtio_cmdq_init_task = kthread_run(virtio_cmdq_init_kthread, NULL,
			"virtio_cmdq_init_kthread");
		buf_initialed = true;
	}
#endif

	return ret;
}

subsys_initcall(virtio_cmdq_init);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("virtio cmdq driver");
MODULE_LICENSE("GPL");
