// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/kvm_host.h>
#include <linux/jiffies.h>
#include <linux/dma-buf.h>
#include <linux/suspend.h>
#include <linux/vhost.h>
#include "virtio_vdmabuf.h"
#include "vhost.h"

/* Only for KVM(QEMU), Just for build pass. unused here */
#define KVM_EVENT_CREATE_VM 0
#define KVM_EVENT_DESTROY_VM 1

#define TIMEOUT_JIFFIES 5

u32 log_level;

/* FIXME */
#define VHOST_VDMABUF_SET_RUNNING       _IOW(VHOST_VIRTIO, 0x7F, int)
#define REFS_PER_PAGE (PAGE_SIZE/sizeof(long))
enum {
	VHOST_VDMABUF_FEATURES = VHOST_FEATURES,
};

static struct virtio_vdmabuf_info *drv_info;
static int hostos_pm_status;
static u64 host_vmid;
static DEFINE_MUTEX(vdmabuf_lock);

struct kvm_instance {
	struct kvm *kvm;
	struct list_head link;
};

struct vhost_vdmabuf {
	struct vhost_dev dev;
	struct vhost_virtqueue vqs[VDMABUF_VQ_MAX];
	struct vhost_work send_work;
	struct virtio_vdmabuf_event_queue *evq;
	u64 vmid;

	struct list_head list;
	struct kvm *kvm;
};

/* Map from Guest PA to host VA. */
static void *map_gpa(struct kvm_vcpu *vcpu/* unused */, gpa_t gpa,
		     size_t size, char *dbg_name)
{
	int n_pages = PAGE_ALIGN(size) >> PAGE_SHIFT, i;
	struct page **pages = vmalloc(sizeof(struct page *) * n_pages);
	struct page **tmp = pages;
	void *vaddr;

	for (i = 0; i < n_pages; i++)
		*tmp++ = phys_to_page(gpa + (i << PAGE_SHIFT));

	vaddr = vmap(pages, n_pages, VM_MAP, PAGE_KERNEL);
	vfree(pages);

	return vaddr;
}

/* gpa from guest-pa to struct page **/
static struct page *map_gpa_to_page(gpa_t gpa)
{
	return phys_to_page(gpa);
}

static void unmap_hva(struct kvm_vcpu *vcpu, gpa_t hva)
{
	vunmap((void *)hva);
}

/* mapping guest's pages for the vdmabuf */
static int
vhost_vdmabuf_map_pages(struct virtio_vdmabuf_shared_pages *pages_info)
{
	struct kvm_vcpu *vcpu = NULL; /* To remove. we are not KVM. */
	int npgs = REFS_PER_PAGE;
	int last_nents, n_l2refs;
	int i, j = 0, k = 0;

	if (!pages_info) {
		pr_notice("%s. invalid input.\n.", __func__);
		return -EINVAL;
	}

	/* Just need to judge pages_info->pages
	 * As long as pages is not NULL, it means that map_pages has
	 * been passed.
	 */
	if (pages_info->pages) {
		pr_info("%s[%d] pages has been existed\n", __func__, __LINE__);
		return 0;
	}

	last_nents = (pages_info->nents - 1) % npgs + 1;
	n_l2refs = (pages_info->nents / npgs) + ((last_nents > 0) ? 1 : 0) -
		    (last_nents == npgs);

	pages_info->pages = kcalloc(pages_info->nents, sizeof(struct page *),
				    GFP_KERNEL);
	if (!pages_info->pages)
		goto fail_page_alloc;

	pages_info->l2refs = kcalloc(n_l2refs, sizeof(gpa_t *), GFP_KERNEL);
	if (!pages_info->l2refs)
		goto fail_l2refs;

	pages_info->l3refs = (gpa_t *)map_gpa(vcpu, pages_info->ref, PAGE_SIZE, "l3ref");
	if (IS_ERR(pages_info->l3refs) || !pages_info->l3refs)
		goto fail_l3refs;

	for (i = 0; i < n_l2refs; i++) {
		pages_info->l2refs[i] = (gpa_t *)map_gpa(vcpu,
							 pages_info->l3refs[i],
							 PAGE_SIZE, "l2ref");

		if (IS_ERR(pages_info->l2refs[i]))
			goto fail_mapping_l2;

		/* last level-2 ref */
		if (i == n_l2refs - 1)
			npgs = last_nents;

		for (j = 0; j < npgs; j++) {
			pages_info->pages[k] = map_gpa_to_page(pages_info->l2refs[i][j]); //virt_to_page(paddr);

			if (j < 2)
				log_info("%s l2ref %d. npgs %d. To page: %pK. from pa 0x%pK.", __func__,
					 i, j, pages_info->pages[k], (void *)pages_info->l2refs[i][j]);
			k++;
		}
		unmap_hva(vcpu, (gpa_t )pages_info->l2refs[i]);
	}

	unmap_hva(vcpu, (gpa_t )pages_info->l3refs);

	/* pages_info->l2refs won't be used. only pages_info->pages will be
	 * used by masters.
	 */
	kfree(pages_info->l2refs);

	return 0;

fail_mapping_l2:
	for (j = 0; j < i; j++) {
		for (k = 0; k < REFS_PER_PAGE; k++)
			unmap_hva(vcpu, (gpa_t )pages_info->l2refs[i][k]);
	}

	unmap_hva(vcpu, (gpa_t )pages_info->l3refs[i]);
	unmap_hva(vcpu, (gpa_t )pages_info->ref);

fail_l3refs:
	kfree(pages_info->l2refs);

fail_l2refs:
	kfree(pages_info->pages);

fail_page_alloc:
	return -ENOMEM;
}

/* unmapping mapped pages */
static int
vhost_vdmabuf_unmap_pages(struct virtio_vdmabuf_shared_pages *pages_info)
{
	/* pages_info->pages will be freed in vhost_vdmabuf_dmabuf_release
	 * function.
	 */
	if (!pages_info || pages_info->pages)
		return -EINVAL;

	return 0;
}

static int vhost_vdmabuf_add_event(struct vhost_vdmabuf *vdmabuf,
				   struct virtio_vdmabuf_buf *buf_info)
{
	struct virtio_vdmabuf_event *e_oldest, *e_new;
	struct virtio_vdmabuf_event_queue *evq = vdmabuf->evq;
	unsigned long irqflags;

	e_new = kzalloc(sizeof(*e_new), GFP_KERNEL);
	if (!e_new)
		return -ENOMEM;

	e_new->e_data.hdr.buf_id = buf_info->buf_id;
	e_new->e_data.data = (void *)buf_info->priv;
	e_new->e_data.hdr.size = buf_info->sz_priv;

	spin_lock_irqsave(&evq->e_lock, irqflags);

	/* check current number of event then if it hits the max num (32)
	 * then remove the oldest event in the list
	 */
	if (evq->pending > 31) {
		e_oldest = list_first_entry(&evq->e_list,
					    struct virtio_vdmabuf_event, link);
		list_del(&e_oldest->link);
		evq->pending--;
		kfree(e_oldest);
	}

	list_add_tail(&e_new->link, &evq->e_list);

	evq->pending++;

	wake_up_interruptible(&evq->e_wait);
	spin_unlock_irqrestore(&evq->e_lock, irqflags);

	return 0;
}

static int send_msg_to_guest(enum virtio_vdmabuf_cmd cmd,
			     struct virtio_vdmabuf_buf_id_t *buf_id)
{
	struct virtio_vdmabuf_msg *msg;
	struct vhost_vdmabuf *vdmabuf;

	if (cmd != VIRTIO_VDMABUF_CMD_DMABUF_REL)
		return -EINVAL;

	msg = kvcalloc(1, sizeof(struct virtio_vdmabuf_msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	memcpy(&msg->op[0], buf_id, sizeof(*buf_id));
	msg->cmd = cmd;

	log_info("%s %d send release cmd.bufid: 0x%llx-%x-%x.\n", __func__, __LINE__,
		((__u64)msg->op[1] << 32 | msg->op[0] ), msg->op[2], msg->op[3]);

	spin_lock(&drv_info->msg_spinlock);
	list_add_tail(&msg->list, &drv_info->msg_list);
	spin_unlock(&drv_info->msg_spinlock);

	mutex_lock(&vdmabuf_lock);
	vdmabuf = drv_info->vdmabuf;
	mutex_unlock(&vdmabuf_lock);
	if (!vdmabuf) {
		dev_notice(drv_info->dev,
			   "can't find vdmabuf for : vmid = 0x%llx\n", host_vmid);
		return -EINVAL;
	}

	/* vhost_work_queue(&vdmabuf->dev, &vdmabuf->send_work); */
	/* RECV: host to guest */
	vhost_vq_work_queue(&vdmabuf->vqs[VDMABUF_VQ_RECV], &vdmabuf->send_work);

	return 0;
}

static int register_exported(struct vhost_vdmabuf *vdmabuf,
			     struct virtio_vdmabuf_buf_id_t *buf_id, int *ops)
{
	struct virtio_vdmabuf_buf *imp;
	int ret;

	imp = kcalloc(1, sizeof(*imp), GFP_KERNEL);
	if (!imp)
		return -ENOMEM;

	imp->pages_info = kcalloc(1, sizeof(struct virtio_vdmabuf_shared_pages),
				  GFP_KERNEL);
	if (!imp->pages_info) {
		kfree(imp);
		return -ENOMEM;
	}

	imp->sz_priv = ops[VIRTIO_VDMABUF_PRIVATE_DATA_SIZE];
	if (imp->sz_priv) {
		imp->priv = kcalloc(1, ops[VIRTIO_VDMABUF_PRIVATE_DATA_SIZE],
					GFP_KERNEL);
		if (!imp->priv) {
			kfree(imp->pages_info);
			kfree(imp);
			return -ENOMEM;
		}
		/* transferring private data */
		memcpy(imp->priv, &ops[VIRTIO_VDMABUF_PRIVATE_DATA_START],
			imp->sz_priv);
	}

	memcpy(&imp->buf_id, buf_id, sizeof(*buf_id));

	imp->pages_info->nents = ops[VIRTIO_VDMABUF_NUM_PAGES_SHARED];
	imp->pages_info->first_ofst = ops[VIRTIO_VDMABUF_FIRST_PAGE_DATA_OFFSET];
	imp->pages_info->last_len = ops[VIRTIO_VDMABUF_LAST_PAGE_DATA_LENGTH];
	imp->pages_info->ref = *(gpa_t *)&ops[VIRTIO_VDMABUF_REF_ADDR_UPPER_32BIT];
	imp->vmid = vdmabuf->vmid;
	mutex_init(&imp->lock);
	INIT_LIST_HEAD(&imp->sg_list);
	mutex_init(&imp->sg_list_lock);
	imp->imported = false;
	pr_info("%s %d. addbuf: bufid: 0x%llx-key %x-%x..vmid %llx.\n",
		__func__, __LINE__,
		buf_id->id, buf_id->rng_key[0], buf_id->rng_key[1],
		vdmabuf->vmid);

	mutex_lock(&drv_info->hash_mutex);
	virtio_vdmabuf_add_buf(drv_info, imp);
	mutex_unlock(&drv_info->hash_mutex);

	/* generate import event */
	ret = vhost_vdmabuf_add_event(vdmabuf, imp);
	if (ret)
		return ret;

	return 0;
}

static void send_to_recvq(struct vhost_vdmabuf *vdmabuf,
			  struct vhost_virtqueue *vq)
{
	struct virtio_vdmabuf_msg *msg;
	int head, in = 0, out = 0, in_size;
	int ret;
	unsigned long timeout;

	mutex_lock(&vq->mutex);

	if (!vhost_vq_get_backend(vq)) {
		pr_notice("%s %d. %p no get backend", __func__, __LINE__, vq);
		goto out;
	}

	vhost_disable_notify(&vdmabuf->dev, vq);

	for (;;) {
		spin_lock(&drv_info->msg_spinlock);
		if (list_empty(&drv_info->msg_list)){
			spin_unlock(&drv_info->msg_spinlock);
			goto out;
		}
		spin_unlock(&drv_info->msg_spinlock);

		timeout = jiffies + msecs_to_jiffies(TIMEOUT_JIFFIES * 200);
		while (vhost_vq_avail_empty(&vdmabuf->dev, vq)) {
			if (hostos_pm_status == PM_SUSPEND_PREPARE || time_after(jiffies, timeout))
				goto out;
		}
		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov),
					 &out, &in, NULL, NULL);

		if (head < 0 || head == vq->num) {
			pr_notice("%s %d. skip head %d. vq->num %d.", __func__, __LINE__,
				  head, vq->num);
			goto out;
		}

		in_size = iov_length(&vq->iov[out], in);
		if (in_size != sizeof(struct virtio_vdmabuf_msg)) {
			dev_notice(drv_info->dev, "rx msg with wrong size\n");
			goto out;
		}

		spin_lock(&drv_info->msg_spinlock);
		msg = list_first_entry(&drv_info->msg_list,
				       struct virtio_vdmabuf_msg, list);

		list_del_init(&msg->list);
		spin_unlock(&drv_info->msg_spinlock);

		ret = __copy_to_user(vq->iov[out].iov_base, msg,
				     sizeof(struct virtio_vdmabuf_msg));
		if (ret) {
			dev_err(drv_info->dev, "fail to copy tx msg\n");
			goto out;
		}

		vhost_add_used(vq, head, in_size);

		kfree(msg);
		vhost_signal(&vdmabuf->dev, vq);
	}

out:
	vhost_enable_notify(&vdmabuf->dev, vq);
	mutex_unlock(&vq->mutex);
}

static void vhost_send_msg_work(struct vhost_work *work)
{
	struct vhost_vdmabuf *vdmabuf = container_of(work,
						     struct vhost_vdmabuf,
						     send_work);
	struct vhost_virtqueue *vq = &vdmabuf->vqs[VDMABUF_VQ_RECV];

	send_to_recvq(vdmabuf, vq);
}

/* parse incoming message from a guest */
static int parse_msg(struct vhost_vdmabuf *vdmabuf,
		     struct virtio_vdmabuf_msg *msg)
{
	struct virtio_vdmabuf_buf_id_t *buf_id;
	struct virtio_vdmabuf_msg *vmid_msg;
	int ret = 0;

	switch (msg->cmd) {
	case VIRTIO_VDMABUF_CMD_EXPORT:
		log_info("%s %d. cmd export.cmdsz 0x%lx.", __func__, __LINE__, sizeof (*msg));
		buf_id = (struct virtio_vdmabuf_buf_id_t *)msg->op;
		ret = register_exported(vdmabuf, buf_id, msg->op);
		if (ret < 0) /* release android shared dmabuf while addbuf fail*/
			send_msg_to_guest(VIRTIO_VDMABUF_CMD_DMABUF_REL, buf_id);
		else
			wake_up_interruptible(&drv_info->vdmabuf_buf_wait_queue);
		break;
	case VIRTIO_VDMABUF_CMD_NEED_VMID:
		log_info("%s %d. cmd need_vmid.", __func__, __LINE__);
		vmid_msg = kvcalloc(1, sizeof(struct virtio_vdmabuf_msg),
				    GFP_KERNEL);
		if (!vmid_msg) {
			ret = -ENOMEM;
			break;
		}

		vmid_msg->cmd = msg->cmd;
		vmid_msg->op[0] = vdmabuf->vmid;
		spin_lock(&drv_info->msg_spinlock);
		list_add_tail(&vmid_msg->list, &drv_info->msg_list);
		spin_unlock(&drv_info->msg_spinlock);
		/* vhost_work_queue(&vdmabuf->dev, &vdmabuf->send_work); */
		vhost_vq_work_queue(&vdmabuf->vqs[VDMABUF_VQ_RECV], &vdmabuf->send_work);

		break;
	default:
		pr_err("%s %d cmd from guest error: 0x%x.", __func__, __LINE__, msg->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void vhost_vdmabuf_handle_send_kick(struct vhost_work *work)
{
	struct vhost_virtqueue *vq = container_of(work,
						  struct vhost_virtqueue,
						  poll.work);
	struct vhost_vdmabuf *vdmabuf = container_of(vq->dev,
						     struct vhost_vdmabuf,
						     dev);
	struct virtio_vdmabuf_msg msg;
	int head, in = 0, out = 0, in_size;
	bool added = false;
	int ret;

	mutex_lock(&vq->mutex);

	if (!vhost_vq_get_backend(vq)) {
		pr_notice("%s %d. %p no get backend.", __func__, __LINE__, vq);
		goto out;
	}

	vhost_disable_notify(&vdmabuf->dev, vq);
	/* Make sure we will process all pending requests */
	for (;;) {
		head = vhost_get_vq_desc(vq, vq->iov, ARRAY_SIZE(vq->iov),
					 &out, &in, NULL, NULL);

		if (head < 0 || head == vq->num)
			break;

		in_size = iov_length(&vq->iov[in], out);
		if (in_size != sizeof(struct virtio_vdmabuf_msg)) {
			dev_notice(drv_info->dev, "rx msg with wrong size\n");
			break;
		}

		if (__copy_from_user(&msg, vq->iov[in].iov_base, in_size)) {
			dev_notice(drv_info->dev,
				"err: can't get the msg from vq\n");
			break;
		}

		ret = parse_msg(vdmabuf, &msg);
		if (ret)
			dev_notice(drv_info->dev, "msg parse error: %d, cmd: %d\n",
				   ret, msg.cmd);

		vhost_add_used(vq, head, in_size);
		added = true;
	}

	vhost_enable_notify(&vdmabuf->dev, vq);
	if (added)
		vhost_signal(&vdmabuf->dev, vq);

out:
	mutex_unlock(&vq->mutex);
}

/*
 * SEND_VQ: commands from guest to host.
 * RECV_VQ: commands from host to guest. No need kick.
 */
static void vhost_vdmabuf_handle_recv_kick(struct vhost_work *work)
{
/*
 *	struct vhost_virtqueue *vq = container_of(work,
 *						  struct vhost_virtqueue,
 *						  poll.work);
 *	struct vhost_vdmabuf *vdmabuf = container_of(vq->dev,
 *						     struct vhost_vdmabuf,
 *						     dev);
 *
 *	send_to_recvq(vdmabuf, vq);
 *
 *	do nothing, just notify guest processed.
 */
}

static int vhost_vdmabuf_get_kvm(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	struct kvm_instance *instance = NULL;
	struct virtio_vdmabuf_info *drv = container_of(nb,
						       struct virtio_vdmabuf_info,
						       kvm_notifier);

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (instance && event == KVM_EVENT_CREATE_VM) {
		if (data) {
			instance->kvm = data;
			list_add_tail(&instance->link,
				      &drv->kvm_instances);
		} else {
			kfree(instance);
		}
	} else {
		kfree(instance);
	}

	return NOTIFY_OK;
}

static struct kvm *find_kvm_instance(u64 vmid)
{
	struct kvm_instance *instance, *tmp;
	struct kvm *kvm = NULL;

	list_for_each_entry_safe(instance, tmp, &drv_info->kvm_instances,
				 link) {
		if (instance->kvm->userspace_pid == vmid) {
			kvm = instance->kvm;

			list_del(&instance->link);
			kfree(instance);
			break;
		}
	}

	return kvm;
}

static int vhost_vdmabuf_host_open(struct inode *inode, struct file *filp)
{
	struct vhost_vdmabuf *vdmabuf;
	struct vhost_virtqueue **vqs;

	if (!drv_info) {
		pr_notice("vhost-vdmabuf: can't open misc device\n");
		return -EINVAL;
	}

	vdmabuf = kvmalloc(sizeof(*vdmabuf), GFP_KERNEL |
			   __GFP_RETRY_MAYFAIL);
	if (!vdmabuf)
		return -ENOMEM;

	vqs = kvmalloc_array(ARRAY_SIZE(vdmabuf->vqs), sizeof(*vqs),
			     GFP_KERNEL);
	if (!vqs) {
		kvfree(vdmabuf);
		return -ENOMEM;
	}

	vdmabuf->evq = kvmalloc(1 * sizeof(*(vdmabuf->evq)), GFP_KERNEL);
	if (!vdmabuf->evq) {
		kvfree(vdmabuf);
		kvfree(vqs);
		return -ENOMEM;
	}

	vqs[VDMABUF_VQ_SEND] = &vdmabuf->vqs[VDMABUF_VQ_SEND];
	vqs[VDMABUF_VQ_RECV] = &vdmabuf->vqs[VDMABUF_VQ_RECV];
	vdmabuf->vqs[VDMABUF_VQ_SEND].handle_kick = vhost_vdmabuf_handle_send_kick;
	vdmabuf->vqs[VDMABUF_VQ_RECV].handle_kick = vhost_vdmabuf_handle_recv_kick;

	vhost_dev_init(&vdmabuf->dev, vqs, ARRAY_SIZE(vdmabuf->vqs),
		       UIO_MAXIOV, 0, 0, true, NULL);

	vhost_work_init(&vdmabuf->send_work, vhost_send_msg_work);
	mutex_lock(&drv_info->kvm_mutex);
	vdmabuf->kvm = find_kvm_instance(vdmabuf->vmid);
	mutex_unlock(&drv_info->kvm_mutex);

	mutex_init(&vdmabuf->evq->e_readlock);
	spin_lock_init(&vdmabuf->evq->e_lock);

	/* Initialize event queue */
	INIT_LIST_HEAD(&vdmabuf->evq->e_list);
	init_waitqueue_head(&vdmabuf->evq->e_wait);

	dev_info(drv_info->dev, "%s done vmid is 0x%llx. vdmabuf: %p, recvq %p, sendq %p.\n",
		 __func__, vdmabuf->vmid, vdmabuf,
		 &vdmabuf->vqs[VDMABUF_VQ_RECV], &vdmabuf->vqs[VDMABUF_VQ_SEND]);
	/* resetting number of pending events */
	vdmabuf->evq->pending = 0;
	mutex_lock(&vdmabuf_lock);
	vdmabuf->vmid = task_pid_nr(current);
	host_vmid = vdmabuf->vmid;
	drv_info->vdmabuf = vdmabuf;
	mutex_unlock(&vdmabuf_lock);
	filp->private_data = vdmabuf;

	return 0;
}

static void vhost_vdmabuf_flush(struct vhost_vdmabuf *vdmabuf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vdmabuf->vqs); i++)
		if (vdmabuf->vqs[i].handle_kick)
			vhost_dev_flush(vdmabuf->vqs[i].poll.dev);

	vhost_dev_flush(&vdmabuf->dev);
}

static int vhost_vdmabuf_host_release(struct inode *inode, struct file *filp)
{
	struct vhost_vdmabuf *vdmabuf = filp->private_data;
	struct virtio_vdmabuf_event *e, *et;

	if (!vdmabuf)
		return -EINVAL;

	mutex_lock(&vdmabuf_lock);
	if (vdmabuf == drv_info->vdmabuf)
		drv_info->vdmabuf = NULL;
	mutex_unlock(&vdmabuf_lock);

	spin_lock_irq(&vdmabuf->evq->e_lock);
	list_for_each_entry_safe(e, et, &vdmabuf->evq->e_list,
				 link) {
		list_del(&e->link);
		kfree(e);
		vdmabuf->evq->pending--;
	}
	spin_unlock_irq(&vdmabuf->evq->e_lock);
	vhost_vdmabuf_flush(vdmabuf);
	vhost_dev_stop(&vdmabuf->dev);
	vhost_dev_cleanup(&vdmabuf->dev);
	dev_info(drv_info->dev, "%s vmid is 0x%llx. vdmabuf: %p\n",
		 __func__, vdmabuf->vmid, vdmabuf);
	kvfree(vdmabuf->evq);
	kvfree(vdmabuf->dev.vqs);
	kvfree(vdmabuf);

	return 0;
}

static unsigned int vhost_vdmabuf_host_event_poll(struct file *filp,
					     struct poll_table_struct *wait)
{
	struct vhost_vdmabuf *vdmabuf = filp->private_data;
	unsigned long irqflags;
	int list_empty_ret;

	if (!vdmabuf)
		return 0;

	poll_wait(filp, &vdmabuf->evq->e_wait, wait);

	spin_lock_irqsave(&vdmabuf->evq->e_lock, irqflags);
	list_empty_ret = list_empty(&vdmabuf->evq->e_list);
	spin_unlock_irqrestore(&vdmabuf->evq->e_lock, irqflags);

	if (!list_empty_ret)
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t vhost_vdmabuf_host_event_read(struct file *filp, char __user *buf,
					     size_t cnt, loff_t *ofst)
{
	struct vhost_vdmabuf *vdmabuf = filp->private_data;
	int ret;

	if (!vdmabuf)
		return -EINVAL;

	if (task_pid_nr(current) != vdmabuf->vmid) {
		dev_notice(drv_info->dev, "current process cannot read events\n");
		return -EPERM;
	}

	/* make sure user buffer can be written */
	if (!access_ok(buf, sizeof(*buf))) {
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

			spin_lock_irq(&vdmabuf->evq->e_lock);
			ret = wait_event_interruptible(vdmabuf->evq->e_wait,
					!list_empty(&vdmabuf->evq->e_list));
			spin_unlock_irq(&vdmabuf->evq->e_lock);

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

			spin_lock_irq(&vdmabuf->evq->e_lock);
			vdmabuf->evq->pending--;
			spin_unlock_irq(&vdmabuf->evq->e_lock);
			kfree(e);
		}
	}

	mutex_unlock(&vdmabuf->evq->e_readlock);

	return ret;
}

static int vhost_vdmabuf_start(struct vhost_vdmabuf *vdmabuf)
{
	struct vhost_virtqueue *vq;
	int i, ret;

	mutex_lock(&vdmabuf->dev.mutex);

	ret = vhost_dev_check_owner(&vdmabuf->dev);
	if (ret)
		goto err;

	for (i = 0; i < ARRAY_SIZE(vdmabuf->vqs); i++) {
		vq = &vdmabuf->vqs[i];

		mutex_lock(&vq->mutex);

		if (!vhost_vq_access_ok(vq)) {
			ret = -EFAULT;
			goto err_vq;
		}

		if (!vhost_vq_get_backend(vq)) {
			vhost_vq_set_backend(vq, vdmabuf);
			ret = vhost_vq_init_access(vq);
			if (ret)
				goto err_vq;
		}

		mutex_unlock(&vq->mutex);
	}

	mutex_unlock(&vdmabuf->dev.mutex);
	log_info("%s %d. vqs_nr %lu. suc.", __func__, __LINE__, ARRAY_SIZE(vdmabuf->vqs));
	return 0;

err_vq:
	vhost_vq_set_backend(vq, NULL);
	mutex_unlock(&vq->mutex);

	for (i = 0; i < ARRAY_SIZE(vdmabuf->vqs); i++) {
		vq = &vdmabuf->vqs[i];

		mutex_lock(&vq->mutex);
		vhost_vq_set_backend(vq, NULL);
		mutex_unlock(&vq->mutex);
	}

err:
	mutex_unlock(&vdmabuf->dev.mutex);
	log_info("%s %d. failed ret %d..", __func__, __LINE__, ret);
	return ret;
}

static int vhost_vdmabuf_stop(struct vhost_vdmabuf *vdmabuf)
{
	struct vhost_virtqueue *vq;
	int i, ret;

	mutex_lock(&vdmabuf->dev.mutex);

	ret = vhost_dev_check_owner(&vdmabuf->dev);
	if (ret)
		goto err;

	for (i = 0; i < ARRAY_SIZE(vdmabuf->vqs); i++) {
		vq = &vdmabuf->vqs[i];

		mutex_lock(&vq->mutex);
		vhost_vq_set_backend(vq, NULL);
		mutex_unlock(&vq->mutex);
	}

err:
	mutex_unlock(&vdmabuf->dev.mutex);
	return ret;
}

static int vhost_vdmabuf_set_features(struct vhost_vdmabuf *vdmabuf,
				      u64 features)
{
	struct vhost_virtqueue *vq;
	int i;

	if (features & ~VHOST_VDMABUF_FEATURES)
		return -EOPNOTSUPP;

	mutex_lock(&vdmabuf->dev.mutex);
	if ((features & (1 << VHOST_F_LOG_ALL)) &&
		!vhost_log_access_ok(&vdmabuf->dev)) {
		mutex_unlock(&vdmabuf->dev.mutex);
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(vdmabuf->vqs); i++) {
		vq = &vdmabuf->vqs[i];
		mutex_lock(&vq->mutex);
		vq->acked_features = features;
		mutex_unlock(&vq->mutex);
	}

	mutex_unlock(&vdmabuf->dev.mutex);
	return 0;
}

/* wrapper ioctl for vhost interface control */
static int vhost_vdmabuf_host_hyp_ioctl(struct file *filp, unsigned int cmd,
					unsigned long param)
{
	struct vhost_vdmabuf *vdmabuf = filp->private_data;
	void __user *argp = (void __user *)param;
	u64 features;
	int ret, start;

	switch (cmd) {
	case VHOST_GET_FEATURES:
		features = VHOST_VDMABUF_FEATURES;
		if (copy_to_user(argp, &features, sizeof(features)))
			return -EFAULT;
		return 0;
	case VHOST_SET_FEATURES:
		if (copy_from_user(&features, argp, sizeof(features)))
			return -EFAULT;
		return vhost_vdmabuf_set_features(vdmabuf, features);
	case VHOST_VDMABUF_SET_RUNNING:
		if (copy_from_user(&start, argp, sizeof(start)))
			return -EFAULT;

		pr_info("%s %d. set running start 0x%x.", __func__, __LINE__, start);
		if (start)
			return vhost_vdmabuf_start(vdmabuf);
		else
			return vhost_vdmabuf_stop(vdmabuf);
	default:
		mutex_lock(&vdmabuf->dev.mutex);
		ret = vhost_dev_ioctl(&vdmabuf->dev, cmd, argp);
		if (ret == -ENOIOCTLCMD)
			ret = vhost_vring_ioctl(&vdmabuf->dev, cmd, argp);
		else
			vhost_vdmabuf_flush(vdmabuf);

		mutex_unlock(&vdmabuf->dev.mutex);
	}

	return ret;
}

struct virtio_vdmabuf_buf *vhost_vdmabuf_get_buf(void *data)
{
	struct virtio_vdmabuf_import *attr = data;
	struct virtio_vdmabuf_buf *imp;
	int retry = 200, ret;

	/* It is best not to have a mutex in the condition of the
	 * wait_event_interruptible_timeout function. Move the judgment
	 * of the condition outside the function to reduce the time of a
	 * single try and retry several times to ensure that the message
	 * can be received in time.
	 * If the single time is too long, the user will experience a
	 * noticeable delay. If the number of retries is too short, the
	 * timeout time will be shortened. Therefore, this time is
	 * retry*single time.
	 */
	mutex_lock(&drv_info->hash_mutex);
	do {
		imp = virtio_vdmabuf_find_buf(drv_info, &attr->buf_id);
		mutex_unlock(&drv_info->hash_mutex);
		wait_event_interruptible_timeout(drv_info->vdmabuf_buf_wait_queue,
						 imp, msecs_to_jiffies(TIMEOUT_JIFFIES));
		mutex_lock(&drv_info->hash_mutex);
	} while (!imp && --retry);
	mutex_unlock(&drv_info->hash_mutex);

	if (!imp) {
		ret = -ENOENT;
		dev_notice(drv_info->dev,
			   "import: no valid buf found with id = 0x%llx-key %x-%x.\n",
			   attr->buf_id.id, attr->buf_id.rng_key[0], attr->buf_id.rng_key[1]);
		goto fail_find;
	}

	ret = vhost_vdmabuf_map_pages(imp->pages_info);
	if (ret < 0) {
		pr_err("%s[%d] failed to map guest pages, ret %d\n", __func__,
			__LINE__, ret);
		goto fail_map;
	}

	log_info("%s[%d] get buf suc, buf_id: 0x%llx-key %x-%x.\n", __func__,
		__LINE__, attr->buf_id.id, attr->buf_id.rng_key[0],
		attr->buf_id.rng_key[1]);
	return imp;

fail_map:
	vhost_vdmabuf_release_buf(&imp->buf_id);
fail_find:
	return ERR_PTR(ret);

}

int vhost_vdmabuf_release_buf(struct virtio_vdmabuf_buf_id_t *buf_id)
{
	struct virtio_vdmabuf_buf *imp;
	int ret;

	if (!buf_id)
		return -EINVAL;

	mutex_lock(&drv_info->hash_mutex);
	imp = virtio_vdmabuf_del_buf(drv_info, buf_id);
	if (IS_ERR(imp)) {
		mutex_unlock(&drv_info->hash_mutex);
		dev_notice(drv_info->dev,
			   "release: no valid buf found with id = %llu-key %x-%x.\n",
			   buf_id->id, buf_id->rng_key[0], buf_id->rng_key[1]);
		return PTR_ERR(imp);
	}

	vhost_vdmabuf_unmap_pages(imp->pages_info);
	mutex_unlock(&drv_info->hash_mutex);

	ret = send_msg_to_guest(VIRTIO_VDMABUF_CMD_DMABUF_REL, buf_id);
	if (ret) {
		dev_notice(drv_info->dev, "fail to send release cmd\n");
		return ret;
	}

	return 0;
}

static long vhost_vdmabuf_host_ioctl(struct file *filp, unsigned int cmd,
				unsigned long param)
{
	int ret;

	/* check if cmd is vhost's */
	if (_IOC_TYPE(cmd) != VHOST_VIRTIO)
		return -EINVAL;

	ret = vhost_vdmabuf_host_hyp_ioctl(filp, cmd, param);

	return ret;
}

static const struct file_operations vhost_vdmabuf_host_fops = {
	.owner = THIS_MODULE,
	.open = vhost_vdmabuf_host_open,
	.release = vhost_vdmabuf_host_release,
	.read = vhost_vdmabuf_host_event_read,
	.poll = vhost_vdmabuf_host_event_poll,
	.unlocked_ioctl = vhost_vdmabuf_host_ioctl,
};

static struct miscdevice vhost_vdmabuf_host_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vhost-vdmabuf-host",
	.fops = &vhost_vdmabuf_host_fops,
};

/* When "android suspend" cannot respond to Yocto msg, Yocto will wait
 * until the timeout.
 * So, register a pm callback here, detect suspend, and directly end the
 * message sending.
 */
static int vhost_vdmabuf_pm_event_handler(struct notifier_block *nb,
					  unsigned long action, void *data)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		hostos_pm_status = PM_SUSPEND_PREPARE;
		break;
	case PM_POST_SUSPEND:
		hostos_pm_status = PM_POST_SUSPEND;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block vdmabuf_pm_notifier = {
	.notifier_call = vhost_vdmabuf_pm_event_handler,
};

static int __init vhost_vdmabuf_init(void)
{
	int ret = 0;

	ret = misc_register(&vhost_vdmabuf_host_miscdev);
	if (ret) {
		pr_notice("vhost-vdmabuf: driver can't be registered\n");
		goto out;
	}

	drv_info = kcalloc(1, sizeof(*drv_info), GFP_KERNEL);
	if (!drv_info) {
		misc_deregister(&vhost_vdmabuf_host_miscdev);
		return -ENOMEM;
	}

	drv_info->dev = vhost_vdmabuf_host_miscdev.this_device;

	hash_init(drv_info->buf_list);
	mutex_init(&drv_info->hash_mutex);

	INIT_LIST_HEAD(&drv_info->msg_list);
	spin_lock_init(&drv_info->msg_spinlock);

	INIT_LIST_HEAD(&drv_info->kvm_instances);
	mutex_init(&drv_info->kvm_mutex);

	drv_info->kvm_notifier.notifier_call = vhost_vdmabuf_get_kvm;
	/* ret = kvm_vm_register_notifier(&drv_info->kvm_notifier); */
	init_waitqueue_head(&drv_info->vdmabuf_buf_wait_queue);

	register_pm_notifier(&vdmabuf_pm_notifier);
	ret = vhost_vdmabuf_dmabuf_init();
	if (ret) {
		pr_err("%s[%d] init failed\n", __func__, __LINE__);
		goto out;
	}

	pr_info("%s[%d] done\n", __func__, __LINE__);

out:
	return ret;
}

static void __exit vhost_vdmabuf_deinit(void)
{
	misc_deregister(&vhost_vdmabuf_host_miscdev);
	vhost_vdmabuf_dmabuf_deinit();

	/* kvm_vm_unregister_notifier(&drv_info->kvm_notifier); */
	kfree(drv_info);
	drv_info = NULL;
}

module_init(vhost_vdmabuf_init);
module_exit(vhost_vdmabuf_deinit);
MODULE_IMPORT_NS(DMA_BUF);

module_param(log_level, uint, 0640);
MODULE_PARM_DESC(log_level, "vdmabuf log level");

MODULE_DESCRIPTION("Vhost Vdmabuf Driver");
MODULE_LICENSE("GPL and additional rights");
