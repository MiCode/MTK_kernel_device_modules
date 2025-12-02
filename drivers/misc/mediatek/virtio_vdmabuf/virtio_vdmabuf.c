// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#define pr_fmt(fmt)    "virtio-vdmabuf: " fmt

#include <linux/init.h>
#include <linux/dma-buf.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <uapi/linux/virtio_ids.h>

#include "virtio_vdmabuf.h"

static struct task_struct *virtio_vdmabuf_thread_handle;

#define VIRTIO_VDMABUF_MAX_ID INT_MAX
#define REFS_PER_PAGE (PAGE_SIZE/sizeof(long))
#define NEW_BUF_ID_GEN(vmid, cnt) (((vmid & 0xFFFFFFFF) << 32) | \
				    ((cnt) & 0xFFFFFFFF))

static u32 log_level;
enum vdmabuf_log_level {
	log_normal_workflow = 1,
};

#define log_info(fmt, ...) \
	do { \
		if (log_level & 1 << log_normal_workflow) \
			pr_info(fmt, ##__VA_ARGS__); \
	} while (0)

#define TIMEOUT_JIFFIES 100

/* one global drv object */
static struct virtio_vdmabuf_info *drv_info;

struct virtio_vdmabuf {
	/* virtio device structure */
	struct virtio_device *vdev;

	/* virtual queue array */
	struct virtqueue *vqs[VDMABUF_VQ_MAX];

	/* ID of guest OS */
	u64 vmid;

	/* spin lock that needs to be acquired before accessing
	 * virtual queue
	 */
	spinlock_t vq_lock;
	struct mutex recv_lock;
	struct mutex send_lock;

	/* for msg_list protect */
	spinlock_t msg_lock;
	struct list_head msg_list;

	/* workqueue */
	struct workqueue_struct *wq;
	struct work_struct recv_work;
	struct work_struct send_work;
	struct work_struct send_msg_work;

	struct virtio_vdmabuf_event_queue *evq;
};

static struct virtio_vdmabuf_buf *virtio_vdmabuf_buf_prepare(void);
static void virtio_vdmabuf_buf_unprepare(struct virtio_vdmabuf_buf *exp);
static void virtio_vdmabuf_unattach_dmabufheap(struct virtio_vdmabuf_buf *exp);

static struct virtio_vdmabuf_buf_id_t get_buf_id(struct virtio_vdmabuf *vdmabuf)
{
	struct virtio_vdmabuf_buf_id_t buf_id = {0, {0, 0} };
	static int count;

	count = count < VIRTIO_VDMABUF_MAX_ID ? count + 1 : 0;
	buf_id.id = NEW_BUF_ID_GEN(vdmabuf->vmid, count);

	/* random data embedded in the id for security */
	get_random_bytes(&buf_id.rng_key[0], 8);

	return buf_id;
}

/* sharing pages for original DMABUF with Host */
static int virtio_vdmabuf_share_buf(struct virtio_vdmabuf_buf *exp)
{
	struct virtio_vdmabuf_shared_pages *pages_info = exp->pages_info;
	struct page **pages = pages_info->pages;
	int nents = pages_info->nents;
	struct sg_page_iter page_iter;
	int n_l2refs = nents/REFS_PER_PAGE +
		       ((nents % REFS_PER_PAGE) ? 1 : 0);
	int i, j = 0;

	pages_info->l3refs = (gpa_t *)__get_free_page(GFP_KERNEL);

	if (!pages_info->l3refs) {
		kvfree(pages_info);
		return -ENOMEM;
	}

	pages_info->l2refs = (gpa_t **)__get_free_pages(GFP_KERNEL,
					get_order(n_l2refs * PAGE_SIZE));

	if (!pages_info->l2refs) {
		free_page((gpa_t)pages_info->l3refs);
		kvfree(pages_info);
		return -ENOMEM;
	}

	/* Share physical address of pages */
	if (exp->is_dmabuf_heap_buf) {
		for_each_sgtable_page(exp->d_sgt, &page_iter, 0) {
			pages_info->l2refs[j++] = (gpa_t *)page_to_phys(
							sg_page_iter_page(&page_iter));
		}
		if (j != nents)
			pr_notice("%s %d. j(%d) != nents(%d).", __func__, __LINE__, j, nents);
	} else {
		for (i = 0; i < nents; i++)
			pages_info->l2refs[i] = (gpa_t *)page_to_phys(pages[i]);
	}

	for (i = 0; i < 2; i++) { /* only for debug print pa. */
		log_info("%s isdmaheap %d(nents: %d). l2ref %d/%d pa %pK.\n", __func__,
			 exp->is_dmabuf_heap_buf, j, i, nents, pages_info->l2refs[i]);
	}

	for (i = 0; i < n_l2refs; i++)
		pages_info->l3refs[i] =
			virt_to_phys((void *)pages_info->l2refs +
				     i * PAGE_SIZE);

	pages_info->ref = (gpa_t)virt_to_phys(pages_info->l3refs);

	return 0;
}

/* stop sharing pages */
static void
virtio_vdmabuf_free_shared_buf(struct virtio_vdmabuf_shared_pages *pages_info)
{
	int n_l2refs = (pages_info->nents/REFS_PER_PAGE +
		       ((pages_info->nents % REFS_PER_PAGE) ? 1 : 0));

	free_pages((gpa_t)pages_info->l2refs, get_order(n_l2refs * PAGE_SIZE));
	free_page((gpa_t)pages_info->l3refs);
}

static int send_msg_to_host(enum virtio_vdmabuf_cmd cmd, int *op)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;
	struct virtio_vdmabuf_msg *msg;
	int i;

	switch (cmd) {
	case VIRTIO_VDMABUF_CMD_NEED_VMID:
		msg = kvcalloc(1, sizeof(struct virtio_vdmabuf_msg),
			       GFP_KERNEL);
		if (!msg)
			return -ENOMEM;

		if (op)
			for (i = 0; i < 4; i++)
				msg->op[i] = op[i];
		break;

	case VIRTIO_VDMABUF_CMD_EXPORT:
		msg = kvcalloc(1, sizeof(struct virtio_vdmabuf_msg),
			       GFP_KERNEL);
		if (!msg)
			return -ENOMEM;

		memcpy(&msg->op[0], &op[0], 10 * sizeof(int) + op[9]);
		break;

	default:
		/* no command found */
		return -EINVAL;
	}

	msg->cmd = cmd;
	spin_lock(&vdmabuf->msg_lock);
	list_add_tail(&msg->list, &vdmabuf->msg_list);
	spin_unlock(&vdmabuf->msg_lock);
	queue_work(vdmabuf->wq, &vdmabuf->send_msg_work);

	return 0;
}

static int add_event_buf_rel(struct virtio_vdmabuf_buf *buf_info)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;
	struct virtio_vdmabuf_event *e_oldest, *e_new;
	struct virtio_vdmabuf_event_queue *eq = vdmabuf->evq;
	unsigned long irqflags;

	e_new = kvzalloc(sizeof(*e_new), GFP_KERNEL);
	if (!e_new)
		return -ENOMEM;

	log_info("%s for bufid: %llx-%x-%x.\n", __func__,
		 buf_info->buf_id.id, buf_info->buf_id.rng_key[0],buf_info->buf_id.rng_key[1]);
	e_new->e_data.hdr.buf_id = buf_info->buf_id;
	e_new->e_data.data = (void *)buf_info->priv;
	e_new->e_data.hdr.size = buf_info->sz_priv;

	spin_lock_irqsave(&eq->e_lock, irqflags);

	/* check current number of events and if it hits the max num (32)
	 * then remove the oldest event in the list
	 */
	if (eq->pending > 31) {
		e_oldest = list_first_entry(&eq->e_list,
					    struct virtio_vdmabuf_event, link);
		list_del(&e_oldest->link);
		eq->pending--;
		kvfree(e_oldest);
	}

	list_add_tail(&e_new->link, &eq->e_list);

	eq->pending++;

	wake_up_interruptible(&eq->e_wait);
	spin_unlock_irqrestore(&eq->e_lock, irqflags);

	return 0;
}

static void virtio_vdmabuf_clear_buf(struct virtio_vdmabuf_buf *exp)
{
	/* Start cleanup of buffer in reverse order to exporting */
	virtio_vdmabuf_free_shared_buf(exp->pages_info);

	if (exp->attach)
		dma_buf_unmap_attachment(exp->attach, exp->sgt,
					DMA_BIDIRECTIONAL);

	if (exp->dma_buf) {
		dma_buf_detach(exp->dma_buf, exp->attach);
		/* close connection to dma-buf completely */
		dma_buf_put(exp->dma_buf);
		exp->dma_buf = NULL;
	}
}

static int remove_buf(struct virtio_vdmabuf *vdmabuf,
		      struct virtio_vdmabuf_buf *exp)
{
	int ret;

	if (exp->is_dmabuf_heap_buf) {
		virtio_vdmabuf_unattach_dmabufheap(exp);
		dma_buf_put(exp->d_dmabuf);
	}

	log_info("%s %d.\n", __func__, __LINE__);
	virtio_vdmabuf_clear_buf(exp);

	ret = virtio_vdmabuf_del_buf(drv_info, &exp->buf_id);
	if (ret)
		dev_err(drv_info->dev, "%s fail %d.\n", __func__, ret);

	if (exp->sz_priv > 0 && !exp->priv)
		kvfree(exp->priv);

	virtio_vdmabuf_buf_unprepare(exp);
	return 0;
}

static int parse_msg_from_host(struct virtio_vdmabuf *vdmabuf,
			       struct virtio_vdmabuf_msg *msg)
{
	struct virtio_vdmabuf_buf *exp;
	struct virtio_vdmabuf_buf_id_t buf_id;
	bool release_async = false;
	int ret;

	switch (msg->cmd) {
	case VIRTIO_VDMABUF_CMD_NEED_VMID:
		vdmabuf->vmid = msg->op[0];
		dev_info(drv_info->dev, "%s get vmid %llx.\n",
			 __func__, vdmabuf->vmid);

		break;
	case VIRTIO_VDMABUF_CMD_DMABUF_REL:
		memcpy(&buf_id, msg->op, sizeof(buf_id));

		exp = virtio_vdmabuf_find_buf(drv_info, &buf_id);
		if (!exp) {
			dev_notice(drv_info->dev, "%s can't find buffer for bufid 0x%llx-%x-%x.\n",
				   __func__, buf_id.id, buf_id.rng_key[0],buf_id.rng_key[1]);

			return -EINVAL;
		}

		dev_info(drv_info->dev, "%s release cmd for bufid 0x%llx-%x-%x.\n",
			 __func__, buf_id.id, buf_id.rng_key[0],buf_id.rng_key[1]);
		if (release_async) {
			ret = add_event_buf_rel(exp);
		} else { /* release immediately */
			ret = remove_buf(vdmabuf, exp);
		}
		if (ret) {
			dev_notice(drv_info->dev, "release a buffer %lld syn %d. ret %d.\n",
				   buf_id.id, !release_async, ret);
			return ret;
		}

		break;
	case VIRTIO_VDMABUF_CMD_EXPORT:
		dev_notice(drv_info->dev, "%s begin to cmd_export.\n", __func__);
		break;
	default:
		dev_notice(drv_info->dev, "%s empty cmd.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void virtio_vdmabuf_recv_work(struct work_struct *work)
{
	struct virtio_vdmabuf *vdmabuf =
		container_of(work, struct virtio_vdmabuf, recv_work);
	struct virtqueue *vq = vdmabuf->vqs[VDMABUF_VQ_RECV];
	struct virtio_vdmabuf_msg *msg;
	struct scatterlist sg;
	int sz, ret;

	mutex_lock(&vdmabuf->recv_lock);

	do {
		virtqueue_disable_cb(vq);
		for (;;) {
			msg = virtqueue_get_buf(vq, &sz);
			if (!msg)
				break;

			/* valid size */
			if (sz == sizeof(struct virtio_vdmabuf_msg)) {
				if (parse_msg_from_host(vdmabuf, msg))
					dev_notice(drv_info->dev,
						   "msg parse error\n");

				kvfree(msg);
			} else {
				dev_notice(drv_info->dev,
					   "received malformed message\n");
			}

			msg = kvzalloc(sizeof(*msg), GFP_KERNEL);
			if (!msg)
				break;
			sg_init_one(&sg, msg, sizeof(struct virtio_vdmabuf_msg));
			ret = virtqueue_add_inbuf(vq, &sg, 1, msg, GFP_KERNEL);
			if (ret)
				dev_notice(drv_info->dev,
					   "virtqueue_add_inbuf fail, ret = %d\n", ret);
		}
	} while (!virtqueue_enable_cb(vq));

	virtqueue_kick(vq);
	mutex_unlock(&vdmabuf->recv_lock);
}

static void virtio_vdmabuf_send_msg_work(struct work_struct *work)
{
	struct virtio_vdmabuf *vdmabuf = container_of(work,
						      struct virtio_vdmabuf,
						      send_msg_work);
	struct scatterlist msg_sg;
	struct scatterlist *sg[1] = {&msg_sg};
	struct virtio_vdmabuf_msg *msg;
	unsigned long timeout;
	struct virtqueue *vq;
	bool added = false, is_timeout = false;
	int ret = 0;

	if (!vdmabuf) {
		dev_err(drv_info->dev, "%s skip NULL vdmabuf.\n", __func__);
		return;
	}

	vq = vdmabuf->vqs[VDMABUF_VQ_SEND];
	mutex_lock(&vdmabuf->send_lock);
	if (list_empty(&vdmabuf->msg_list))
		goto out;

	timeout = jiffies + msecs_to_jiffies(TIMEOUT_JIFFIES);
	while (vq->num_free < virtqueue_get_vring_size(vq)) {
		if (time_after(jiffies, timeout)) {
			dev_info(drv_info->dev, "%s[%d] send msg work timeout, vring_size %d, num_free: %d\n",
				 __func__, __LINE__, virtqueue_get_vring_size(vq),
				 vq->num_free);
			is_timeout = true;
			goto err;
		}
	}

	spin_lock(&vdmabuf->msg_lock);
	while (!list_empty(&vdmabuf->msg_list)) {
		msg = list_first_entry(&vdmabuf->msg_list, struct virtio_vdmabuf_msg, list);
		dev_info(drv_info->dev, "%s cmd: %x, bufid: 0x%llx-%x-%x\n", __func__, msg->cmd,
			 ((__u64)msg->op[1] << 32 | msg->op[0]), msg->op[2], msg->op[3]);
		sg_init_one(&msg_sg, msg, sizeof(struct virtio_vdmabuf_msg));
		ret = virtqueue_add_sgs(vq, sg, 1, 0, msg, GFP_ATOMIC);
		if (ret < 0) {
			dev_err(drv_info->dev, "failed to add msg to vq, ret: %d\n", ret);
			spin_unlock(&vdmabuf->msg_lock);
			goto err;
		}

		list_del_init(&msg->list);
		added = true;
	}
	spin_unlock(&vdmabuf->msg_lock);

err:
	if (ret || is_timeout)
		queue_work(vdmabuf->wq, &vdmabuf->send_msg_work);
out:
	if (added)
		virtqueue_kick(vq);
	mutex_unlock(&vdmabuf->send_lock);
}

static void virtio_vdmabuf_send_work(struct work_struct *work)
{
	struct virtio_vdmabuf *vdmabuf =
		container_of(work, struct virtio_vdmabuf, send_work);
	struct virtqueue *vq = vdmabuf->vqs[VDMABUF_VQ_SEND];
	struct virtio_vdmabuf_msg *msg;
	unsigned int sz;

	mutex_lock(&vdmabuf->send_lock);

	do {
		virtqueue_disable_cb(vq);

		for (;;) {
			msg = virtqueue_get_buf(vq, &sz);
			if (!msg)
				break;

			kvfree(msg);
		}
	} while (!virtqueue_enable_cb(vq));

	mutex_unlock(&vdmabuf->send_lock);
}

static void virtio_vdmabuf_recv_cb(struct virtqueue *vq)
{
	struct virtio_vdmabuf *vdmabuf = vq->vdev->priv;

	if (!vdmabuf)
		return;

	queue_work(vdmabuf->wq, &vdmabuf->recv_work);
}

static void virtio_vdmabuf_send_cb(struct virtqueue *vq)
{
	/* SEND VQ is from guest to host. it should not get from host. */
	struct virtio_vdmabuf *vdmabuf = vq->vdev->priv;

	if (!vdmabuf)
		return;

	queue_work(vdmabuf->wq, &vdmabuf->send_work);
}

static int remove_all_bufs(struct virtio_vdmabuf *vdmabuf)
{
	struct virtio_vdmabuf_buf *found;
	struct hlist_node *tmp;
	int bkt;
	int ret;

	log_info("%s[%d] enter\n", __func__, __LINE__);
	hash_for_each_safe(drv_info->buf_list, bkt, tmp, found, node) {
		ret = remove_buf(vdmabuf, found);
		if (ret)
			return ret;
	}

	return 0;
}

static struct sg_table
*virtio_vdmabuf_map_dmabuf(struct dma_buf_attachment *attachment,
			   enum dma_data_direction dir)
{
	struct virtio_vdmabuf_buf *exp_buf;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	int i, ret;

	if (!attachment->dmabuf || !attachment->dmabuf->priv)
		return ERR_PTR(-EINVAL);

	exp_buf = attachment->dmabuf->priv;

	sgt = kvzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(sgt, exp_buf->pages_info->nents, GFP_KERNEL);
	if (ret) {
		kvfree(sgt);
		return ERR_PTR(ret);
	}

	sgl = sgt->sgl;
	for (i = 0; i < exp_buf->pages_info->nents; i++) {
		sg_set_page(sgl, exp_buf->pages_info->pages[i], PAGE_SIZE, 0);
		sgl = sg_next(sgl);
	}

	if (!dma_map_sg(attachment->dev, sgt->sgl, sgt->nents, dir)) {
		sg_free_table(sgt);
		kvfree(sgt);
		return ERR_PTR(-EINVAL);
	}

	return sgt;
}

static int virtio_vdmabuf_mmap_dmabuf(struct dma_buf *dmabuf,
				      struct vm_area_struct *vma)
{
	struct virtio_vdmabuf_buf *exp_buf;
	u64 uaddr;
	int i, ret;

	if (!dmabuf->priv)
		return -EINVAL;

	exp_buf = dmabuf->priv;

	if (!exp_buf->pages_info)
		return -EINVAL;

	/* vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP; */

	uaddr = vma->vm_start;
	log_info("%s %d, try mmap uaddr %llx.\n", __func__, __LINE__, uaddr);
	for (i = 0; i < exp_buf->pages_info->nents; i++) {
		//ret = vm_insert_page(vma, uaddr,
		//		     exp_buf->pages_info->pages[i]);
		ret = remap_pfn_range(vma, uaddr, page_to_pfn(exp_buf->pages_info->pages[i]),
				      PAGE_SIZE, vma->vm_page_prot);
		if (ret) {
			pr_notice("%s %d, i %d uaddr 0x%llx vminsert %d.\n", __func__, __LINE__,
				  i, uaddr, ret);
			return ret;
		}

		uaddr += PAGE_SIZE;
		if (uaddr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static void virtio_vdmabuf_unmap_dmabuf(struct dma_buf_attachment *attachment,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	dma_unmap_sg(attachment->dev, sgt->sgl, sgt->nents, dir);

	sg_free_table(sgt);
	kvfree(sgt);
}

static void virtio_vdmabuf_release_dmabuf(struct dma_buf *dmabuf)
{
	struct virtio_vdmabuf_buf *exp_buf = dmabuf->priv;
	int i;

	log_info("%s %d, fd %d,isdmaheap %d sz 0x%llx..\n",
		 __func__, __LINE__, exp_buf->fd, exp_buf->is_dmabuf_heap_buf, exp_buf->size);
	for (i = 0; i < exp_buf->pages_info->nents; i++)
		put_page(exp_buf->pages_info->pages[i]);
}

static const struct dma_buf_ops virtio_vdmabuf_dmabuf_ops =  {
	.map_dma_buf = virtio_vdmabuf_map_dmabuf,
	.unmap_dma_buf = virtio_vdmabuf_unmap_dmabuf,
	.release = virtio_vdmabuf_release_dmabuf,
	.mmap = virtio_vdmabuf_mmap_dmabuf,
};

static int virtio_vdmabuf_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int virtio_vdmabuf_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct virtio_vdmabuf_buf *virtio_vdmabuf_buf_prepare(void)
{
	struct virtio_vdmabuf_buf *exp_buf;

	exp_buf = kvzalloc(sizeof(*exp_buf), GFP_KERNEL);
	if (!exp_buf)
		return ERR_PTR(-ENOMEM);
	exp_buf->pages_info = kvzalloc(sizeof (*(exp_buf->pages_info)),
				       GFP_KERNEL);
	if (!exp_buf->pages_info) {
		kfree(exp_buf);
		return ERR_PTR(-ENOMEM);
	}
	return exp_buf;
}

static void virtio_vdmabuf_buf_unprepare(struct virtio_vdmabuf_buf *exp)
{
	kvfree(exp->pages_info);
	kvfree(exp);
}

static int virtio_vdmabuf_attach_dmabufheap(struct virtio_vdmabuf_buf *exp_buf)
{
	exp_buf->d_attach = dma_buf_attach(exp_buf->d_dmabuf, drv_info->dev);
	if (IS_ERR(exp_buf->d_attach)) {
		dev_notice(drv_info->dev, "Failed to attach dmabuf\n");
		return PTR_ERR(exp_buf->d_attach);
	}

	exp_buf->d_sgt = dma_buf_map_attachment(exp_buf->d_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(exp_buf->d_sgt)) {
		dev_notice(drv_info->dev, "Failed to map attach dmabuf\n");
		dma_buf_detach(exp_buf->d_dmabuf, exp_buf->d_attach);
		return PTR_ERR(exp_buf->d_sgt);
	}

	return 0;
}

static void virtio_vdmabuf_unattach_dmabufheap(struct virtio_vdmabuf_buf *exp)
{
	dma_buf_unmap_attachment(exp->d_attach, exp->d_sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(exp->d_dmabuf, exp->d_attach);

	exp->d_attach = NULL;
	exp->d_sgt = NULL;
}

/* Notify Host about the new vdmabuf */
static int export_notify(struct virtio_vdmabuf_buf *exp)
{
	struct virtio_vdmabuf_shared_pages *pages_info = exp->pages_info;
	int *op;
	int ret;

	op = kvcalloc(1, sizeof(int) * 65, GFP_KERNEL);
	if (!op)
		return -ENOMEM;

	memcpy(op, &exp->buf_id, sizeof(exp->buf_id));

	op[4] = pages_info->nents;
	op[5] = pages_info->first_ofst;
	op[6] = PAGE_SIZE;

	memcpy(&op[7], &pages_info->ref, sizeof(gpa_t));
	op[9] = exp->sz_priv;

	/* driver/application specific private info */
	memcpy(&op[10], exp->priv, op[9]);

	ret = send_msg_to_host(VIRTIO_VDMABUF_CMD_EXPORT, op);

	kvfree(op);
	return ret;
}

static struct virtio_vdmabuf_buf *
virtio_vdmabuf_get_bufid_internal(struct dma_buf *exp_dmabuf)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;
	struct virtio_vdmabuf_buf *exp;
	struct dma_buf *dbuf = NULL;
	struct device *dev = drv_info->dev;
	bool is_dmabuf_buf = true;
	int ret;

	if (!exp_dmabuf)
		return ERR_PTR(-EINVAL);

	dbuf = exp_dmabuf;

	if (vdmabuf->vmid <= 0) {
		pr_notice("warning: %s %d vmid is 0.\n", __func__, __LINE__);
		/* return -EINVAL; */ /* Asyc command, vmid will be got later. */
	}

	/* To try if this is a valid dmabuf, like from dma-buf heap. */

	mutex_lock(&drv_info->g_mutex);

	exp = virtio_vdmabuf_buf_prepare();
	if (IS_ERR(exp)) {
		ret = PTR_ERR(exp);
		goto g_unlock;
	}

	exp->d_dmabuf = dbuf;
	ret = virtio_vdmabuf_attach_dmabufheap(exp);
	if (ret < 0) {
		virtio_vdmabuf_buf_unprepare(exp);
		dev_err(dev, "%s %d attach_dmabufheap: fail(%d).\n",
			__func__, __LINE__, ret);
		goto g_unlock;
	}

	exp->buf_id = get_buf_id(vdmabuf);
	log_info("export_ioctl(bufid: %llx-key %x-%x)",
		 exp->buf_id.id, exp->buf_id.rng_key[0],
		 exp->buf_id.rng_key[1]);
	exp->pages_info->nents = DIV_ROUND_UP(dbuf->size, PAGE_SIZE);
	exp->is_dmabuf_heap_buf = is_dmabuf_buf;
	exp->size = dbuf->size;

	virtio_vdmabuf_add_buf(drv_info, exp);

	ret = virtio_vdmabuf_share_buf(exp);
	if (ret < 0) {
		dev_err(dev, "%s %d share_buf: fail(%d).\n", __func__, __LINE__, ret);
		goto err_unattach_buf;
	}

	ret = export_notify(exp);
	if (ret < 0) {
		dev_err(dev, "%s %d export_notify: fail(%d).\n", __func__, __LINE__, ret);
		goto free_shared_buf;
	}

	log_info("vdmabuf driver %s %d export suc isdmaheap %d(sz: 0x%llx). vmid %llx.\n",
		 __func__, __LINE__, is_dmabuf_buf, exp->size,
		 vdmabuf->vmid);

	exp->valid = 1;

	mutex_unlock(&drv_info->g_mutex);

	return exp;

free_shared_buf:
	virtio_vdmabuf_free_shared_buf(exp->pages_info);
err_unattach_buf:
	if (is_dmabuf_buf) {
		virtio_vdmabuf_del_buf(drv_info, &exp->buf_id);
		virtio_vdmabuf_unattach_dmabufheap(exp);
		virtio_vdmabuf_buf_unprepare(exp);
	} else {
		kvfree(exp->priv);
	}
g_unlock:
	mutex_unlock(&drv_info->g_mutex);
	if (is_dmabuf_buf)
		dma_buf_put(dbuf);

	return ERR_PTR(ret);
}

int mtk_vdmabuf_vguest_export(struct dma_buf *dmabuf,
			      struct virtio_vdmabuf_buf_id_t *buf_id)
{
	struct virtio_vdmabuf_buf *exp;
	struct task_struct *cur_task = current;

	if (!dmabuf)
		return -EINVAL;

	get_dma_buf(dmabuf);
	exp = virtio_vdmabuf_get_bufid_internal(dmabuf);
	if (IS_ERR(exp)) {
		dev_notice(drv_info->dev, "%s[%d] get_bufid fail, ret: %ld\n", __func__, __LINE__,
			   PTR_ERR(exp));
		return PTR_ERR(exp);
	}

	*buf_id = exp->buf_id;
	dev_info(drv_info->dev, "%s[%d] export done to bufid 0x%llx-%x-%x.\n",
		 __func__, __LINE__,exp->buf_id.id,
		 exp->buf_id.rng_key[0], exp->buf_id.rng_key[1]);
	dev_info(drv_info->dev, "%s[%d] process id: %d, process name: %s, thread id: %d\n",
		 __func__, __LINE__, cur_task->tgid, cur_task->comm, cur_task->pid);

	return 0;
}
EXPORT_SYMBOL(mtk_vdmabuf_vguest_export);

/* ioctl - exporting new vdmabuf
 *
 *	 int dmabuf_fd - File handle of original DMABUF
 *	 virtio_vdmabuf_buf_id_t buf_id - returned vdmabuf ID
 *	 int sz_priv - size of private data from userspace
 *	 char *priv - buffer of user private data
 *
 */
static int export_ioctl(struct file *filp, void *data)
{
	struct virtio_vdmabuf_export *attr = data;
	struct virtio_vdmabuf_buf *exp;
	struct dma_buf *dmabuf;
	struct task_struct *cur_task = current;

	dmabuf = dma_buf_get(attr->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_notice(drv_info->dev, "%s %d buf_fd: %d, dmabuf: %ld.\n",
			   __func__, __LINE__, attr->fd, PTR_ERR(dmabuf));
		return -EINVAL;
	}

	exp = virtio_vdmabuf_get_bufid_internal(dmabuf);
	if (IS_ERR(exp)) {
		pr_err("%s[%d] get_bufid fail, ret: %ld\n", __func__, __LINE__,
		       PTR_ERR(exp));
		return PTR_ERR(exp);
	}

	exp->fd = attr->fd;
	exp->filp = filp;
	attr->buf_id = exp->buf_id;
	dev_info(drv_info->dev, "%s[%d] fd: %d, export done to bufid 0x%llx-%x-%x.\n",
		 __func__, __LINE__, exp->fd, exp->buf_id.id,
		 exp->buf_id.rng_key[0], exp->buf_id.rng_key[1]);
	dev_info(drv_info->dev, "%s[%d] process id: %d, process name: %s, thread id: %d\n",
		 __func__, __LINE__, cur_task->tgid, cur_task->comm, cur_task->pid);

	return 0;
}

static const struct virtio_vdmabuf_ioctl_desc virtio_vdmabuf_ioctls[] = {
	VIRTIO_VDMABUF_IOCTL_DEF(VIRTIO_VDMABUF_IOCTL_EXPORT, export_ioctl, 0),
};

static long virtio_vdmabuf_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long param)
{
	const struct virtio_vdmabuf_ioctl_desc *ioctl = NULL;
	unsigned int nr = _IOC_NR(cmd);
	int ret;
	virtio_vdmabuf_ioctl_t func;
	void *kdata;

	if (nr >= ARRAY_SIZE(virtio_vdmabuf_ioctls)) {
		dev_notice(drv_info->dev, "invalid ioctl\n");
		return -EINVAL;
	}

	ioctl = &virtio_vdmabuf_ioctls[nr];

	func = ioctl->func;

	if (unlikely(!func)) {
		dev_notice(drv_info->dev, "no function\n");
		return -EINVAL;
	}

	kdata = kvmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	if (copy_from_user(kdata, (void __user *)param,
			   _IOC_SIZE(cmd)) != 0) {
		dev_notice(drv_info->dev,
			   "failed to copy from user arguments\n");
		ret = -EFAULT;
		goto ioctl_error;
	}

	ret = func(filp, kdata);

	if (copy_to_user((void __user *)param, kdata,
			 _IOC_SIZE(cmd)) != 0) {
		dev_notice(drv_info->dev,
			   "failed to copy to user arguments\n");
		ret = -EFAULT;
		goto ioctl_error;
	}

ioctl_error:
	kvfree(kdata);
	return ret;
}

static unsigned int virtio_vdmabuf_event_poll(struct file *filp,
					      struct poll_table_struct *wait)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;

	poll_wait(filp, &vdmabuf->evq->e_wait, wait);

	if (!list_empty(&vdmabuf->evq->e_list))
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t virtio_vdmabuf_event_read(struct file *filp, char __user *buf,
					 size_t cnt, loff_t *ofst)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;
	int ret;

	/* make sure user buffer can be written */
	if (!access_ok(buf, sizeof (*buf))) {
		dev_notice(drv_info->dev, "user buffer can't be written.\n");
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&vdmabuf->evq->e_readlock);
	if (ret)
		return ret;

	for (;;) {
		struct virtio_vdmabuf_event *e = NULL;

		spin_lock_irq(&vdmabuf->evq->e_lock);
		if (!list_empty(&vdmabuf->evq->e_list)) {
			e = list_first_entry(&vdmabuf->evq->e_list,
					     struct virtio_vdmabuf_event, link);
			list_del(&e->link);
		}
		spin_unlock_irq(&vdmabuf->evq->e_lock);

		if (!e) {
			if (ret)
				break;

			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			mutex_unlock(&vdmabuf->evq->e_readlock);
			ret = wait_event_interruptible(vdmabuf->evq->e_wait,
					!list_empty(&vdmabuf->evq->e_list));

			if (ret == 0)
				ret = mutex_lock_interruptible(
						&vdmabuf->evq->e_readlock);

			if (ret)
				return ret;
		} else {
			unsigned int len = (sizeof(e->e_data.hdr) +
					    e->e_data.hdr.size);

			if (len > cnt - ret) {
put_back_event:
				spin_lock_irq(&vdmabuf->evq->e_lock);
				list_add(&e->link, &vdmabuf->evq->e_list);
				spin_unlock_irq(&vdmabuf->evq->e_lock);
				break;
			}

			if (copy_to_user(buf + ret, &e->e_data.hdr,
					 sizeof(e->e_data.hdr))) {
				if (ret == 0)
					ret = -EFAULT;

				goto put_back_event;
			}

			ret += sizeof(e->e_data.hdr);

			if (copy_to_user(buf + ret, e->e_data.data,
					 e->e_data.hdr.size)) {
				/* error while copying void *data */

				struct virtio_vdmabuf_e_hdr dummy_hdr = {0};

				ret -= sizeof(e->e_data.hdr);

				/* nullifying hdr of the event in user buffer */
				if (copy_to_user(buf + ret, &dummy_hdr,
						 sizeof(dummy_hdr)))
					dev_notice(drv_info->dev,
						   "fail to nullify invalid hdr\n");

				ret = -EFAULT;

				goto put_back_event;
			}

			ret += e->e_data.hdr.size;
			vdmabuf->evq->pending--;
			kvfree(e);
		}
	}

	mutex_unlock(&vdmabuf->evq->e_readlock);

	return ret;
}

static const struct file_operations virtio_vdmabuf_fops = {
	.owner = THIS_MODULE,
	.open = virtio_vdmabuf_open,
	.release = virtio_vdmabuf_release,
	.read = virtio_vdmabuf_event_read,
	.poll = virtio_vdmabuf_event_poll,
	.unlocked_ioctl = virtio_vdmabuf_ioctl,
};

static struct miscdevice virtio_vdmabuf_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "virtio-vdmabuf",
	.fops = &virtio_vdmabuf_fops,
};

static int vdmabuf_fill_recvq(struct virtio_vdmabuf *vdmabuf)
{
	int ret;
	struct virtqueue *vq = vdmabuf->vqs[VDMABUF_VQ_RECV];
	struct scatterlist sg;
	struct virtio_vdmabuf_msg *msg;

	msg = kvzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	sg_init_one(&sg, msg, sizeof(struct virtio_vdmabuf_msg));
	ret = virtqueue_add_inbuf(vq, &sg, 1, msg, GFP_KERNEL);
	if (ret) {
		dev_err(drv_info->dev, "virtqueue_add_inbuf fail, ret: %d.\n", ret);
		return ret;
	}

	return 0;
}

static int virtio_vdmabuf_probe(struct virtio_device *vdev)
{
	vq_callback_t *cbs[] = {
		virtio_vdmabuf_recv_cb,
		virtio_vdmabuf_send_cb,
	};
	static const char *const names[] = {
		"recv",
		"send",
	};
	struct virtio_vdmabuf *vdmabuf;
	int ret = 0;

	if (!drv_info)
		return -EINVAL;

	vdmabuf = drv_info->priv;

	if (!vdmabuf)
		return -EINVAL;

	vdmabuf->vdev = vdev;
	vdev->priv = vdmabuf;

	/* initialize spinlock for synchronizing virtqueue accesses */
	spin_lock_init(&vdmabuf->vq_lock);

	ret = virtio_find_vqs(vdmabuf->vdev, VDMABUF_VQ_MAX, vdmabuf->vqs,
			      cbs, names, NULL);
	if (ret) {
		dev_notice(drv_info->dev, "Cannot find any vqs\n");
		return ret;
	}

	virtio_device_ready(vdev);
	spin_lock_init(&vdmabuf->msg_lock);
	INIT_LIST_HEAD(&vdmabuf->msg_list);
	INIT_WORK(&vdmabuf->recv_work, virtio_vdmabuf_recv_work);
	INIT_WORK(&vdmabuf->send_work, virtio_vdmabuf_send_work);
	INIT_WORK(&vdmabuf->send_msg_work, virtio_vdmabuf_send_msg_work);
	if (vdmabuf_fill_recvq(vdmabuf))
		dev_err(drv_info->dev, "vdmabuf_fill_recvq fail\n");

	/* vmid only needs to be get once */
	ret = send_msg_to_host(VIRTIO_VDMABUF_CMD_NEED_VMID, 0);
	if (ret < 0)
		dev_notice(drv_info->dev, "CMD_NEED_VMID sent fail\n");

	pr_info("%s %d connected with host. done\n", __func__, __LINE__);
	return ret;
}

static void virtio_vdmabuf_remove(struct virtio_device *vdev)
{
	struct virtio_vdmabuf *vdmabuf;

	if (!drv_info)
		return;

	vdmabuf = drv_info->priv;
	flush_work(&vdmabuf->recv_work);
	flush_work(&vdmabuf->send_work);
	flush_work(&vdmabuf->send_msg_work);

	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
}

static struct virtio_device_id vdmabuf_id_table[] = {
	{ VIRTIO_ID_VDMABUF, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int vdmabuf_features[] = {
	0,
};

static struct virtio_driver virtio_vdmabuf_vdev_drv = {
	.feature_table	= vdmabuf_features,
	.feature_table_size = ARRAY_SIZE(vdmabuf_features),
	.driver.name	=  "mtk-virtio-vdmabuf",
	.driver.owner = THIS_MODULE,
	.id_table =     vdmabuf_id_table,
	.probe =        virtio_vdmabuf_probe,
	.remove =       virtio_vdmabuf_remove,
};

static int virtio_vdmabuf_init_func(void *data)
{
	struct virtio_vdmabuf *vdmabuf;
	int ret = 0;

	drv_info = NULL;

	ret = misc_register(&virtio_vdmabuf_miscdev);
	if (ret) {
		pr_notice("virtio-vdmabuf misc driver can't be registered\n");
		return ret;
	}

	dma_coerce_mask_and_coherent(virtio_vdmabuf_miscdev.this_device,
				     DMA_BIT_MASK(36));

	drv_info = kvcalloc(1, sizeof(*drv_info), GFP_KERNEL);
	if (!drv_info) {
		misc_deregister(&virtio_vdmabuf_miscdev);
		return -ENOMEM;
	}

	vdmabuf = kvcalloc(1, sizeof(*vdmabuf), GFP_KERNEL);
	if (!vdmabuf) {
		kvfree(drv_info);
		misc_deregister(&virtio_vdmabuf_miscdev);
		return -ENOMEM;
	}

	vdmabuf->evq = kvcalloc(1, sizeof(*vdmabuf->evq), GFP_KERNEL);
	if (!vdmabuf->evq) {
		kvfree(drv_info);
		kvfree(vdmabuf);
		misc_deregister(&virtio_vdmabuf_miscdev);
		return -ENOMEM;
	}

	drv_info->priv = (void *)vdmabuf;
	drv_info->dev = virtio_vdmabuf_miscdev.this_device;

	mutex_init(&drv_info->g_mutex);

	mutex_init(&vdmabuf->evq->e_readlock);
	mutex_init(&vdmabuf->recv_lock);
	mutex_init(&vdmabuf->send_lock);
	spin_lock_init(&vdmabuf->evq->e_lock);

	INIT_LIST_HEAD(&vdmabuf->evq->e_list);
	init_waitqueue_head(&vdmabuf->evq->e_wait);
	hash_init(drv_info->buf_list);

	vdmabuf->evq->pending = 0;
	vdmabuf->wq = create_workqueue("virtio_vdmabuf_wq");

	ret = register_virtio_driver(&virtio_vdmabuf_vdev_drv);
	if (ret) {
		dev_notice(drv_info->dev, "vdmabuf driver can't be registered\n");
		misc_deregister(&virtio_vdmabuf_miscdev);
		kvfree(vdmabuf);
		kvfree(drv_info);
		return -EFAULT;
	}

	pr_info("%s %d. done\n", __func__, __LINE__);
	return 0;
}

static int __init virtio_vdmabuf_init(void)
{
	int ret = 0;

	virtio_vdmabuf_thread_handle = kthread_run(virtio_vdmabuf_init_func,
						NULL, "virtio_vdmabuf_init_thread");
	if (IS_ERR(virtio_vdmabuf_thread_handle)) {
		ret = PTR_ERR(virtio_vdmabuf_thread_handle);
		pr_info("virtio_vdmabuf_init_thread run fail, err = %d\n", ret);
	}
	return ret;
}

static void __exit virtio_vdmabuf_deinit(void)
{
	struct virtio_vdmabuf *vdmabuf = drv_info->priv;
	struct virtio_vdmabuf_event *e, *et;
	unsigned long irqflags;

	misc_deregister(&virtio_vdmabuf_miscdev);
	unregister_virtio_driver(&virtio_vdmabuf_vdev_drv);

	if (vdmabuf->wq)
		destroy_workqueue(vdmabuf->wq);

	spin_lock_irqsave(&vdmabuf->evq->e_lock, irqflags);

	list_for_each_entry_safe(e, et, &vdmabuf->evq->e_list,
				 link) {
		list_del(&e->link);
		kvfree(e);
		vdmabuf->evq->pending--;
	}

	spin_unlock_irqrestore(&vdmabuf->evq->e_lock, irqflags);

	/* freeing all exported buffers */
	remove_all_bufs(vdmabuf);

	kvfree(vdmabuf->evq);
	kvfree(vdmabuf);
	kvfree(drv_info);
}

module_init(virtio_vdmabuf_init);
module_exit(virtio_vdmabuf_deinit);
MODULE_IMPORT_NS(DMA_BUF);

module_param(log_level, uint, 0640);
MODULE_PARM_DESC(log_level, "vdmabuf log level");

MODULE_DEVICE_TABLE(virtio, vdmabuf_id_table);
MODULE_DESCRIPTION("Virtio Vdmabuf frontend driver");
MODULE_LICENSE("GPL and additional rights");
