// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
/*
 * Copyright (C) 2022 GoldenRiver Inc.
 */

// Note: Enable the following macro to show debug messages if CONFIG_DYNAMIC_DEBUG is not set
// #define DEBUG

#define pr_fmt(fmt) "virtio-eint: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>

#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/vringh.h>
#include <uapi/linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/dma-mapping.h>
#include <linux/virtio_config.h>
#include <linux/completion.h>
#include <linux/printk.h>
#include <linux/kthread.h>

#include <linux/atomic.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/hypervisor/virtio_thread.h>
#include <uapi/linux/sched/types.h>

#include "virtio-eint.h"

#if defined(CONFIG_EINT_UNITTEST)
#include "eint-unittest.h"
#endif

#define GIRQ_NO_TIMEOUT	    0xffffffff
#define GIRQ_TIMEOUT_DEFAULT 1000
#define GIRQ_MAX_BUF_SIZE    (1024 * 64)
#define VIRTIO_EINT_DEV "virtio_eint"
#define MAX_GIRQ_MSG_SIZE 64
#define RQ_TIMEOUT_MS 1000
enum EINT_DEBUG_LEVEL {
	EINT_DEBUG_LEVEL_NONE = 0,
	EINT_DEBUG_LEVEL_ERROR,
	EINT_DEBUG_LEVEL_INFO,
	EINT_DEBUG_LEVEL_DEBUG,
	EINT_DEBUG_LEVEL_VERBOSE,
};

struct virtio_eint {
	struct virtio_device *vdev;
	struct virtqueue *vqs[GIRQ_VQ_MAX];
	spinlock_t vq_lock[GIRQ_VQ_MAX];
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs_root;
#endif
	struct kthread_worker send_kworker;
	struct kthread_worker kworker;
	struct task_struct *kworker_task;
	struct task_struct *kworker_send_task;
	int enable_debug;
	atomic_t requestId;
	int debug_gpio;
	int *gpio_mark_status;
	bool drop_handler;
};

static struct virtio_eint *g_eint;
static atomic_t initvirt_done = ATOMIC_INIT(0);
typedef void (*irq_handler_cb)(int, int);
static irq_handler_cb irq_handler_func;
typedef int (*find_virq_cb)(int);
static find_virq_cb find_irq_func;

struct virtio_eint_pkt_hdr {
	uint8_t type;
};

struct virtio_eint_pkt {
	struct virtio_eint_pkt_hdr hdr;
	union {
		struct eint_request_packet req;
		struct eint_response_packet res;
		struct eint_event_packet evt;
	} u;
	struct completion done;
	struct list_head list;
	void *out_buf[MAX_GIRQ_IOV_SIZE];
	void *in_buf[MAX_GIRQ_IOV_SIZE];
	bool wait_result;
	struct kthread_work work;
};

int get_virtio_eint_ready(void)
{
	return atomic_read(&initvirt_done);
}
EXPORT_SYMBOL_GPL(get_virtio_eint_ready);

void set_gpio_count(int num)
{
	g_eint->gpio_mark_status = kcalloc(num, sizeof(int), GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(set_gpio_count);

void register_handler_cb(void(*cb)(int, int))
{
	irq_handler_func = cb;
}
EXPORT_SYMBOL_GPL(register_handler_cb);

void register_findirq_cb(int(*cb)(int))
{
	find_irq_func = cb;
}
EXPORT_SYMBOL_GPL(register_findirq_cb);

int get_debug_level(void)
{
	return g_eint->enable_debug;
}
EXPORT_SYMBOL_GPL(get_debug_level);

int get_debug_gpio(void)
{
	return g_eint->debug_gpio;
}
EXPORT_SYMBOL_GPL(get_debug_gpio);

bool get_debug_drop(void)
{
	return g_eint->drop_handler;
}
EXPORT_SYMBOL_GPL(get_debug_drop);


void getvirtio_handler(int hwvirq, int eventId)
{
	if (irq_handler_func)
		irq_handler_func(hwvirq, eventId);
}

int ResretValue;

int getResretValue(void)
{
	return ResretValue;
}
//extern int getvirtio_handler(unsigned int irq);
/**
 * virtio_eint_sg_init - initialize scatterlist according to cpu address location
 * @sg: scatterlist to fill
 * @cpu_addr: virtual address of the buffer
 * @len: buffer length
 *
 * An internal function filling scatterlist according to virtual address
 * location (in vmalloc or in kernel).
 */
static void virtio_eint_sg_init(struct scatterlist *sg, void *cpu_addr,
			       unsigned int len)
{
	if (is_vmalloc_addr(cpu_addr)) {
		sg_init_table(sg, 1);
		sg_set_page(sg, vmalloc_to_page(cpu_addr), len,
			    offset_in_page(cpu_addr));
	} else {
		WARN_ON(!virt_addr_valid(cpu_addr));
		sg_init_one(sg, cpu_addr, len);
	}
}

static void free_eint_pkt_buffer(const void *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}

static inline int alloc_vqueue_buffer(struct virtio_eint_pkt *pkt,
				      size_t buf_len)
{
	pkt->in_buf[0] = kzalloc(buf_len, GFP_ATOMIC);
	if (!pkt->in_buf[0])
		return -ENOMEM;

	return 0;
}

static inline struct virtio_eint_pkt *to_pkt(struct virtio_eint_pkt_hdr *hdr)
{
	return (struct virtio_eint_pkt *)hdr;
}

struct virtio_eint_pkt *alloc_eint_packet(void *pkt_data, uint8_t pkt_type)
{
	struct virtio_eint_pkt *pkt = kzalloc(sizeof(*pkt), GFP_ATOMIC);

	if (pkt == NULL)
		return NULL;

	init_completion(&pkt->done);
	INIT_LIST_HEAD(&pkt->list);

	pkt->hdr.type = pkt_type;
	switch (pkt_type) {
	case GIRQ_VQ_REQ:
		memcpy(&pkt->u.req, pkt_data,
		       sizeof(struct eint_request_packet));
		break;
	case GIRQ_VQ_EVT:
		memcpy(&pkt->u.evt, pkt_data, sizeof(struct eint_event_packet));
		break;
	default:
		pr_info("unknown eint packet type: %u\n", pkt_type);
		goto err;
	}
	return pkt;

err:
	kfree(pkt);
	return NULL;
}

void free_eint_packet(struct virtio_eint_pkt *pkt)
{
	int i;

	for (i = 0; i < MAX_GIRQ_IOV_SIZE; i++) {
		free_eint_pkt_buffer(pkt->in_buf[i]);
		free_eint_pkt_buffer(pkt->out_buf[i]);
	}
	kfree(pkt);
}

static LIST_HEAD(wait_result_list);
static DEFINE_SPINLOCK(wait_result_lock);

#define MAXVIRGPIONUM 10

static void __handle_eint_event(struct eint_event_packet *evt)
{
	getvirtio_handler(evt->num, evt->event_id);
}

static void handle_event_packet(struct kthread_work *work)
{
	struct virtio_eint_pkt *pkt =
		container_of(work, struct virtio_eint_pkt, work);

	__handle_eint_event(&pkt->u.evt);
	kfree(pkt);
}

static void add_event_to_work_queue(struct virtio_eint *eint,
				    struct eint_event_packet *evt)
{
	struct virtio_eint_pkt *pkt = alloc_eint_packet(evt, GIRQ_VQ_EVT);

	if (pkt == NULL)
		return;

	if (eint->enable_debug)
		pr_info("%s: event_type=%u event_id=%u hwirq:%d\n",
		 __func__, evt->event_type, evt->event_id, evt->num);

	kthread_init_work(&pkt->work, handle_event_packet);
	kthread_queue_work(&eint->kworker, &pkt->work);
}

void handle_eint_response_packet(struct virtio_eint_pkt *res_pkt)
{
	struct virtio_eint_pkt *req_pkt, *tmp;
	bool found = false;
	struct eint_request_packet *req;
	struct eint_response_packet *res = &res_pkt->u.res;
	uint32_t request_id = res_pkt->u.res.request_id;

	spin_lock(&wait_result_lock);
	list_for_each_entry_safe(req_pkt, tmp, &wait_result_list, list) {
		req = &req_pkt->u.req;
		if (g_eint->enable_debug)
			pr_info("request_id: %u request_op:%u response_id:%u response_op:%u\n",
				req->request_id, req->op, res->request_id, res->op);
		if ((req->request_id == res->request_id) &&
		    (req->op == res->op)) {
			found = true;
			break;
		}
	}
	spin_unlock(&wait_result_lock);
	ResretValue = res->ret;
	free_eint_packet(res_pkt);

	if (!found) {
		pr_info("BUG: no matching request packet for response packet, request_id=%u\n",
			request_id);
		return;
	}
	complete(&req_pkt->done);
}

/* Called from virtio device, in IRQ context */
void eint_request_done(struct virtqueue *vq)
{
	struct virtio_eint *eint = vq->vdev->priv;
	struct virtio_eint_pkt_hdr *hdr;
	spinlock_t *lock = &eint->vq_lock[GIRQ_VQ_REQ];
	unsigned long flags;
	unsigned int len;
	bool notify = false;

	spin_lock_irqsave(lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_eint_pkt *pkt = to_pkt(hdr);

			handle_eint_response_packet(pkt);
		}

		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	if (virtqueue_kick_prepare(vq))
		notify = true;

	spin_unlock_irqrestore(lock, flags);

	if (notify)
		virtqueue_notify(vq);
}

static int queue_eint_packet(struct virtqueue *vq, struct virtio_eint_pkt *pkt,
			    uint8_t pkt_type)
{
	struct scatterlist pkt_sg, pkt_sg_in;
	struct scatterlist *sgs[2] = { &pkt_sg, &pkt_sg_in };
	int ret;

	switch (pkt_type) {
	case GIRQ_VQ_REQ:
		virtio_eint_sg_init(&pkt_sg, &pkt->u.req, sizeof(pkt->u.req));
		virtio_eint_sg_init(&pkt_sg_in, &pkt->u.res, sizeof(pkt->u.res));
		ret = virtqueue_add_sgs(vq, sgs, 1, 1, pkt, GFP_ATOMIC);
		break;
	case GIRQ_VQ_EVT:
		virtio_eint_sg_init(&pkt_sg_in, &pkt->u.evt, sizeof(pkt->u.evt));
		sgs[0] = &pkt_sg_in;
		ret = virtqueue_add_sgs(vq, sgs, 0, 1, pkt, GFP_ATOMIC);
		break;
	default:
		pr_info("%s: unknown eint packet type: %u\n", __func__, pkt_type);
		ret = -EINVAL;
	}

	if (ret) {
		pr_info("%s: failed to queue eint packet, ret=%d\n", __func__,
			ret);
	}
	return ret;
}

/* Called from virtio device, in IRQ context */
void eint_event_done(struct virtqueue *vq)
{
	struct virtio_eint *eint = vq->vdev->priv;
	struct virtio_eint_pkt_hdr *hdr;
	spinlock_t *lock = &eint->vq_lock[GIRQ_VQ_EVT];
	unsigned long flags;
	unsigned int len;
	int ret;

	spin_lock_irqsave(lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((hdr = virtqueue_get_buf(vq, &len)) != NULL) {
			struct virtio_eint_pkt *pkt = to_pkt(hdr);

			add_event_to_work_queue(eint, &pkt->u.evt);
			ret = queue_eint_packet(vq, pkt, GIRQ_VQ_EVT);
			if (ret) {
				pr_info("failed to enqueue event buffer, ret=%d\n",
				       ret);
			}
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	spin_unlock_irqrestore(&eint->vq_lock[GIRQ_VQ_EVT], flags);
	virtqueue_notify(vq);
}

int fill_eint_packets(struct virtio_eint *eint, uint8_t pkt_type)
{
	struct virtio_eint_pkt *pkt;
	struct virtqueue *vq = eint->vqs[pkt_type];
	size_t nr_sgs = vq->num_free;
	int ret = 0;

	pr_debug("preparing virtio-eint vq buffer: type %u nbufs %zd\n", pkt_type,
		 nr_sgs);
	do {
		pkt = kzalloc(sizeof(*pkt), GFP_ATOMIC);
		if (!pkt)
			break;

		switch (pkt_type) {
		case GIRQ_VQ_EVT:
			ret = queue_eint_packet(vq, pkt, pkt_type);
			nr_sgs -= 1;
			break;
		default:
			pr_info("unknown eint packet type: %u\n", pkt_type);
			ret = -EINVAL;
		}

		if (ret) {
			pr_info("%s: failed to queue eint packet, type=%u, ret=%d\n",
			       __func__, pkt_type, ret);
			free_eint_packet(pkt);
			goto failed;
		}
	} while (nr_sgs > 0);

	virtqueue_kick(vq);
failed:
	return ret;
}

static int __submit_req(struct virtio_eint_pkt *pkt, bool wait_result)
{
	unsigned long flags, flage;
	bool notify = false;
	unsigned long timeout;
	int ret;
	struct virtio_eint_pkt *pkt_cache;

	struct virtqueue *vq = g_eint->vqs[GIRQ_VQ_REQ];
	spinlock_t *lock = &g_eint->vq_lock[GIRQ_VQ_REQ];

	if (g_eint == NULL) {
		pr_info("%s: virtio-eint driver un-initialized\n", __func__);
		return -ENODEV;
	}

	if (pkt == NULL) {
		pr_info("%s: eint packet is NULL\n", __func__);
		return -EINVAL;
	}

	if (g_eint->enable_debug)
		pr_info("%s request_id=%u, request_op=%u, hwirq:%d\n", __func__,
			pkt->u.req.request_id, pkt->u.req.op, pkt->u.req.num);

	spin_lock_irqsave(lock, flags);

	ret = queue_eint_packet(vq, pkt, GIRQ_VQ_REQ);
	if (ret) {
		spin_unlock_irqrestore(lock, flags);
		ret = -ENOMEM;
		goto out;
	}

	if (virtqueue_kick_prepare(vq))
		notify = true;
	spin_unlock_irqrestore(lock, flags);

	pkt_cache = alloc_eint_packet(&pkt->u.req, GIRQ_VQ_REQ);
	spin_lock_irqsave(&wait_result_lock, flage);
	pkt_cache->wait_result = wait_result;
	if (list_empty(&pkt_cache->list))
		list_add_tail(&pkt_cache->list, &wait_result_list);
	spin_unlock_irqrestore(&wait_result_lock, flage);

	if (notify)
		virtqueue_notify(vq);

	timeout = wait_for_completion_timeout(&pkt_cache->done, msecs_to_jiffies(RQ_TIMEOUT_MS));
	if (!timeout) {
		pr_info("%s: wait timed out for pkt %p (req_id: %u), removing from list\n",
			__func__, pkt_cache, pkt_cache->u.req.request_id);
		ret = -ETIMEDOUT;
	}
	spin_lock_irqsave(&wait_result_lock, flage);
	if (!list_empty(&pkt_cache->list)) {
		list_del_init(&pkt_cache->list);
		free_eint_packet(pkt_cache);
	}
	spin_unlock_irqrestore(&wait_result_lock, flage);

out:
	return ret;
}

static void handle_send_packet(struct kthread_work *work)
{
	struct virtio_eint_pkt *pkt =
		container_of(work, struct virtio_eint_pkt, work);

	__submit_req(pkt, false);
	// free_eint_packet(pkt);
}

static int submit_simple_request(struct eint_request_packet *req)
{
	struct virtio_eint_pkt *pkt;
	int i = 0;

	/* assume the simple request have no extra buffer */
	for (i = 0; i < MAX_GIRQ_IOV_SIZE; i++) {
		req->in_buf_size[i] = 0;
		req->out_buf_size[i] = 0;
	}
	req->ret = 0;

	if (req->num == 0 || req->request_type == 0) {
		pr_err("eintgpio get req num or type fail\n");
		return -1;
	}

	pkt = alloc_eint_packet(req, GIRQ_VQ_REQ);
	if (pkt == NULL) {
		pr_err("eint pkt alloc fail\n");
		return -ENOMEM;
	}

	kthread_init_work(&pkt->work, handle_send_packet);
	kthread_queue_work(&g_eint->send_kworker, &pkt->work);
	return 0;
}

int submit_cmd(struct irq_data *d, int cmdId, int type, unsigned int dbc)
{
	struct eint_request_packet req_st;
	struct irq_desc *desc = irq_data_to_desc(d);

	if (!atomic_read(&initvirt_done)) {
		pr_err("%s irq:%u hwirq:%lu error for virtio eint not init\n"
			, __func__, d->irq, d->hwirq);
		return -EFAULT;
	}

	switch (cmdId) {
	case EINT_OPS_SUBMIT_NORMAL:
		break;
	case EINT_OPS_SUBMIT_UNMASK:
		if (g_eint->enable_debug > EINT_DEBUG_LEVEL_ERROR)
			pr_info("%s irq:%u hwirq:%lu\n", __func__, d->irq, d->hwirq);
		if (!g_eint->gpio_mark_status[d->hwirq]) {
			pr_debug("%s: hwirq:%lu skip, not user control\n", __func__, d->hwirq);
			return 0;
		}
		g_eint->gpio_mark_status[d->hwirq] = 0;
		break;
	case EINT_OPS_SUBMIT_MASK:
		if (g_eint->enable_debug > EINT_DEBUG_LEVEL_ERROR)
			pr_info("%s irq:%u hwirq:%lu irq_depth:%d\n", __func__, d->irq, d->hwirq, desc->depth);
		if (!desc->depth) {
			pr_debug("%s: hwirq:%lu skip,not user control\n", __func__, d->hwirq);
			return 0;
		}
		g_eint->gpio_mark_status[d->hwirq] = 1;
		break;
	default:
		break;
	}

	req_st.request_id = atomic_read(&g_eint->requestId);
	req_st.num = d->hwirq;
	req_st.request_type = type;
	req_st.debonce = dbc;
	req_st.op = cmdId;
	req_st.ret = 0;
	atomic_inc(&g_eint->requestId);

	return submit_simple_request(&req_st);
}
EXPORT_SYMBOL_GPL(submit_cmd);

#define MAX_PANIC_MSG_SIZE 1024

void virt_eint_set_test1(int a)
{
	struct eint_request_packet req = {
		.request_id = 0,
		.num = 20,
		.request_type = 1,
		.debonce = 0,
		.op = 0,
		.ret = 0,
	};
	submit_simple_request(&req);
}

void virt_eint_set_test(int sdio_flag)
{
	virt_eint_set_test1(sdio_flag);
}

static int run_test_show(struct seq_file *m, void *v)
{
	return 0;
}

static int run_test_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, run_test_show, inode->i_private);
}

static ssize_t run_test_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *p = kmalloc(count + 1, GFP_ATOMIC);
	int ret;

	if (p == NULL)
		return -ENOMEM;

	memset(p, 0, count + 1);
	if (copy_from_user(p, buf, count)) {
		pr_info("failed to get data from user\n");
		ret = -EFAULT;
		goto out;
	}

	if (strncmp(p, "test1", 5) == 0) {
		int a = 5;

		virt_eint_set_test(a);
	} else if (strncmp(p, "test2", 5) == 0) {
		virt_eint_set_test1(6);
	} else {
		ret = -EINVAL;
		pr_info("unknown test command: %s\n", p);
	}

out:
	kfree(p);
	return count;
}

static ssize_t virtioeint_debug_show(struct device_driver *driver, char *buf)
{
	unsigned int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"virtio_eint debug enable:%u drop_gpio:%d, drop:%d\n",
			 g_eint->enable_debug, g_eint->debug_gpio, g_eint->drop_handler);

	return strlen(buf);

}
static ssize_t virtioeint_debug_store(struct device_driver *driver,
				     const char *buf, size_t count)
{
	int sel, val, ret;
	int virq;

	ret = sscanf(buf, "%d %d", &sel, &val);
	if (ret != 2) {
		dev_info(&g_eint->vdev->dev, "%s invalid input: %s.\n", __func__, buf);
		goto err_out;
	}

	switch (sel) {
	case 0:
		g_eint->enable_debug = val;
		pr_info("Set debug_log_enable to %d\n", !!val);
		break;
	case 1:
		g_eint->debug_gpio = val;
		pr_info("Set debug_gpio to %d\n",g_eint->debug_gpio);
		break;
	case 2:
		if (find_irq_func) {
			virq = find_irq_func(g_eint->debug_gpio);
			if (val)
				enable_irq(virq);
			else
				disable_irq(virq);

			pr_info("mask:%d gpio:%d virq:%d\n",val, g_eint->debug_gpio, virq);
		}
		break;
	case 3:
		g_eint->drop_handler = !!val;
		pr_info("Set drop_handler to %d\n", !!val);
		break;
	default:
		dev_info(&g_eint->vdev->dev, "Unknown selector: %d\n", sel);
		return -EINVAL;
	}

err_out:
	return count;
}

static DRIVER_ATTR_RW(virtioeint_debug);
static const struct file_operations run_test_fops = {
	.open = run_test_open,
	.release = single_release,
	.read = seq_read,
	.write = run_test_write,
	.owner = THIS_MODULE,
};

static int virtio_eint_probe(struct virtio_device *vdev)
{
	// vq_callback_t *vq_cbs[GIRQ_VQ_MAX] = { eint_request_done, eint_event_done };
	//const char *names[GIRQ_VQ_MAX] = { "request", "event" };
	struct virtio_eint *eint;
	struct sched_param sp = { .sched_priority = 50 };
	int err = -EINVAL;
	int i;
#if defined(CONFIG_VIRTIO_EINT_SETAFFINITY)
	cpumask_t mask;
#endif

	struct virtqueue_info vqs_info[GIRQ_VQ_MAX] = {
		[0] = {
			.name = "request",
			.callback = eint_request_done,
			.ctx = false,
		},
		[1] = {
			.name = "event",
			.callback = eint_event_done,
			.ctx = false,
		},
	};

	eint = kzalloc(sizeof(*eint), GFP_ATOMIC);
	if (!eint)
		return -ENOMEM;

	eint->vdev = vdev;
	for (i = 0; i < GIRQ_VQ_MAX; i++)
		spin_lock_init(&eint->vq_lock[i]);

	kthread_init_worker(&eint->kworker);
	eint->kworker_task = kthread_run(kthread_worker_fn, &eint->kworker,
					"eint-event-worker");
	if (IS_ERR(eint->kworker_task)) {
		dev_info(&vdev->dev, "failed to create event worker task\n");
		err = PTR_ERR(eint->kworker_task);
		goto err_exit;
	}
	sched_setscheduler_nocheck(eint->kworker_task, SCHED_FIFO, &sp);

	kthread_init_worker(&eint->send_kworker);
	eint->kworker_send_task = kthread_run(kthread_worker_fn, &eint->send_kworker,
					"eint-send-worker");
	if (IS_ERR(eint->kworker_send_task)) {
		dev_info(&vdev->dev, "failed to create event worker task\n");
		err = PTR_ERR(eint->kworker_send_task);
		goto err_exit;
	}

	err = virtio_find_vqs(vdev, GIRQ_VQ_MAX, eint->vqs, vqs_info, NULL);
	if (err) {
		dev_info(&vdev->dev, "failed to find virtio VQs, err=%d\n",
			 err);
		goto err_wq;
	}

#if defined(CONFIG_VIRTIO_EINT_SETAFFINITY)
	cpumask_clear(&mask);
	cpumask_set_cpu(6, &mask);
	for (i = 0; i < GIRQ_VQ_MAX; i++)
		virtqueue_set_affinity(eint->vqs[i], &mask);
#endif

	virtio_device_ready(vdev);

	//mdelay(300);
	err = fill_eint_packets(eint, GIRQ_VQ_EVT);
	if (err)
		goto err_find_vq;

	vdev->priv = eint;
	g_eint = eint;

#if defined(CONFIG_DEBUG_FS)
	eint->debugfs_root = debugfs_create_dir(VIRTIO_EINT_DEV, NULL);
	if (IS_ERR(eint->debugfs_root)) {
		pr_info("debugfs_create_dir failed: %ld\n",
		       PTR_ERR(eint->debugfs_root));
		err = PTR_ERR(eint->debugfs_root);
		goto err_notify;
	}

	debugfs_create_file("run_test", 0600, eint->debugfs_root, NULL,
			    &run_test_fops);
#endif

	atomic_set(&initvirt_done, 1);
	g_eint->enable_debug = EINT_DEBUG_LEVEL_ERROR;
	g_eint->drop_handler = false;
	atomic_set(&g_eint->requestId, 0);
	err = driver_create_file(eint->vdev->dev.driver,
				  &driver_attr_virtioeint_debug);
	if (err)
		pr_err("Failed to create virtio eint debug sysfs\n");

	pr_info("virtio eint initialized\n");
	return 0;
#if defined(CONFIG_DEBUG_FS)
err_notify:
#endif
err_find_vq:
	if (eint->vdev)
		vdev->config->del_vqs(eint->vdev);
err_wq:
	kthread_flush_worker(&eint->kworker);
	kthread_flush_worker(&eint->send_kworker);
	kthread_stop(eint->kworker_task);
	kthread_stop(eint->kworker_send_task);
err_exit:
	kfree(eint);
	g_eint = NULL;
	atomic_set(&initvirt_done, 0);
	dev_info(&vdev->dev, "virtio eint probe failed:%d\n", err);
	return err;
}

static void virtio_eint_remove(struct virtio_device *vdev)
{
	struct virtio_eint *eint = vdev->priv;
#if defined(CONFIG_DEBUG_FS)
	debugfs_remove_recursive(eint->debugfs_root);
#endif
	//atomic_notifier_chain_unregister(&vmctl_notifier_list, &eint_vmctl_notifier);

	vdev->config->reset(vdev);
	vdev->vringh_config->del_vrhs(eint->vdev);
	vdev->config->del_vqs(eint->vdev);
	kthread_flush_worker(&eint->kworker);
	kthread_flush_worker(&eint->send_kworker);
	kthread_stop(eint->kworker_task);
	kthread_stop(eint->kworker_send_task);

	kfree(g_eint->gpio_mark_status);
	kfree(eint);

	g_eint = NULL;
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_SUBSYSTEM_ID_MTK_EINT, PCI_SUBVENDOR_ID_MTK },
	{ 0 },
};

static unsigned int features[] = {};

static struct virtio_driver virtio_eint_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = id_table,
	.probe = virtio_eint_probe,
	.remove = virtio_eint_remove,
};

module_virtio_driver_thread(virtio_eint_driver);
MODULE_DEVICE_TABLE(virtio, id_table);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtio eint device driver");
MODULE_AUTHOR("Gerd Hoffmann <kraxel@redhat.com>");

