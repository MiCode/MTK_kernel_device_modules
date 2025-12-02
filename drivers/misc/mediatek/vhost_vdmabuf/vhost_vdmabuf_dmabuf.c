// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#define pr_fmt(fmt)    "vhost-vdmabuf: " fmt

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/vhost.h>
#include <linux/vfio.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include "virtio_vdmabuf.h"
#include "vhost.h"

#define REFS_PER_PAGE (PAGE_SIZE/sizeof(long))

/* create sg_table with given pages and other parameters */
static struct sg_table *new_sgt(struct page **pgs,
				int first_ofst, int last_len,
				int nents)
{
	struct sg_table *sgt;
	struct scatterlist *sgl;
	int i, ret;

	sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(sgt, nents, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(-ENOMEM);
	}

	sgl = sgt->sgl;
	sg_set_page(sgl, pgs[0], PAGE_SIZE-first_ofst, first_ofst);

	for (i = 1; i < nents-1; i++) {
		sgl = sg_next(sgl);
		sg_set_page(sgl, pgs[i], PAGE_SIZE, 0);
	}

	/* more than 1 page */
	if (nents > 1) {
		sgl = sg_next(sgl);
		sg_set_page(sgl, pgs[i], last_len, 0);
	}

	return sgt;
}

static void vhost_vdmabuf_dmabuf_release_sglist(struct virtio_vdmabuf_buf *imp)
{
	struct virtio_vdmabuf_sg_entry *sg_entry, *tmp;

	mutex_lock(&imp->sg_list_lock);
	if (!list_empty(&imp->sg_list)) {
		list_for_each_entry_safe(sg_entry, tmp, &imp->sg_list, list) {
			sg_free_table(sg_entry->sgt);
			kfree(sg_entry->sgt);
			list_del(&sg_entry->list);
			kfree(sg_entry);
		}
	}
	mutex_unlock(&imp->sg_list_lock);
}

static struct sg_table
*vhost_vdmabuf_dmabuf_map(struct dma_buf_attachment *attachment,
			  enum dma_data_direction dir)
{
	struct virtio_vdmabuf_buf *imp;
	struct virtio_vdmabuf_sg_entry *sg_entry;
	struct sg_table *sgtable;

	if (!attachment->dmabuf || !attachment->dmabuf->priv)
		return ERR_PTR(-EINVAL);

	imp = (struct virtio_vdmabuf_buf *)attachment->dmabuf->priv;

		sg_entry = kzalloc(sizeof(*sg_entry), GFP_KERNEL);
	if (!sg_entry)
		return ERR_PTR(-ENOMEM);

	sgtable = new_sgt(imp->pages_info->pages, imp->pages_info->first_ofst,
			  imp->pages_info->last_len, imp->pages_info->nents);
	if (IS_ERR(sgtable)) {
		dev_err(attachment->dev, "%s new-sgt fail.\n", __func__);
		kfree(sg_entry);
		return sgtable;
	}

	sg_entry->sgt = sgtable;
	if (!dma_map_sg(attachment->dev, sg_entry->sgt->sgl,
			sg_entry->sgt->nents, dir)) {
		dev_err(attachment->dev, "%s dma_map_sg fail.\n", __func__);
		sg_free_table(sg_entry->sgt);
		kfree(sg_entry->sgt);
		kfree(sg_entry);
		return NULL;
	}

	sg_entry->dev = attachment->dev;
	mutex_lock(&imp->sg_list_lock);
	list_add(&sg_entry->list, &imp->sg_list);
	mutex_unlock(&imp->sg_list_lock);
	return sg_entry->sgt;
}

static void
vhost_vdmabuf_dmabuf_unmap(struct dma_buf_attachment *attachment,
			   struct sg_table *sgt,
			   enum dma_data_direction dir)
{
	dma_unmap_sg(attachment->dev, sgt->sgl, sgt->nents, dir);
}

static int vhost_vdmabuf_dmabuf_mmap(struct dma_buf *dmabuf,
				     struct vm_area_struct *vma)
{
	struct virtio_vdmabuf_buf *imp;
	u64 uaddr;
	int i, err;

	if (!dmabuf->priv)
		return -EINVAL;

	imp = (struct virtio_vdmabuf_buf *)dmabuf->priv;

	if (!imp->pages_info)
		return -EINVAL;

	/* FIXME */
	/* vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP; */

	uaddr = vma->vm_start;

	log_info("%s %d enter uaddr 0x%llx.\n", __func__, __LINE__, uaddr);
	for (i = 0; i < imp->pages_info->nents; i++) {
		err = vm_insert_page(vma, uaddr,
				     imp->pages_info->pages[i]);
		if (err) {
			pr_notice("%s %d, i %d uaddr %llx vminsert %d.\n", __func__, __LINE__, i, uaddr, err);
			return err;
		}

		uaddr += PAGE_SIZE;
		if (uaddr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static int vhost_vdmabuf_dmabuf_vmap(struct dma_buf *dmabuf,
				     struct iosys_map *map)
{
	struct virtio_vdmabuf_buf *imp;
	void *addr;

	if (!dmabuf->priv)
		return -EINVAL;

	imp = (struct virtio_vdmabuf_buf *)dmabuf->priv;
	if (!imp->pages_info)
		return -EINVAL;

	mutex_lock(&imp->lock);
	if (imp->vmap_cnt) {
		imp->vmap_cnt++;
		iosys_map_set_vaddr(map, imp->vaddr);
		goto out_unlock;
	}

	addr = vmap(imp->pages_info->pages, imp->pages_info->nents,
		    0, PAGE_KERNEL);
	if (IS_ERR(addr)) {
		mutex_unlock(&imp->lock);
		pr_err("%s %d vmap fail %ld.\n", __func__, __LINE__, PTR_ERR(addr));
		return PTR_ERR(addr);
	}

	imp->vaddr = addr;
	imp->vmap_cnt++;
	iosys_map_set_vaddr(map, imp->vaddr);

out_unlock:
	mutex_unlock(&imp->lock);
	return 0;
}

static void vhost_vdmabuf_dmabuf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct virtio_vdmabuf_buf *imp;

	if (!dmabuf->priv)
		return;
	imp = (struct virtio_vdmabuf_buf *)dmabuf->priv;

	mutex_lock(&imp->lock);
	if (!--imp->vmap_cnt) {
		vunmap(imp->vaddr);
		imp->vaddr = NULL;
	}
	mutex_unlock(&imp->lock);
	iosys_map_clear(map);
}

static void vhost_vdmabuf_dmabuf_release(struct dma_buf *dma_buf)
{
	struct virtio_vdmabuf_buf *imp;

	if (!dma_buf->priv)
		return;

	imp = (struct virtio_vdmabuf_buf *)dma_buf->priv;
	vhost_vdmabuf_release_buf(&imp->buf_id);

	imp->dma_buf = NULL;
	/* release sg_table */
	vhost_vdmabuf_dmabuf_release_sglist(imp);
	pr_info("%s %d release bufid 0x%llx.\n", __func__, __LINE__,
		 imp->buf_id.id);
	kfree(imp->priv);
	kfree(imp->pages_info->pages);
	kfree(imp->pages_info);
	kfree(imp);
}

static const struct dma_buf_ops vhost_vdmabuf_dmabuf_ops = {
	.map_dma_buf = vhost_vdmabuf_dmabuf_map,
	.unmap_dma_buf = vhost_vdmabuf_dmabuf_unmap,
	.release = vhost_vdmabuf_dmabuf_release,
	.mmap = vhost_vdmabuf_dmabuf_mmap,
	.vmap = vhost_vdmabuf_dmabuf_vmap,
	.vunmap = vhost_vdmabuf_dmabuf_vunmap,
};

/* exporting dmabuf as fd */
static int vhost_vdmabuf_export(struct virtio_vdmabuf_buf *imp)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int ret = 0;

	exp_info.ops = &vhost_vdmabuf_dmabuf_ops;
	/* multiple of PAGE_SIZE, not considering offset */
	exp_info.size = imp->pages_info->nents * PAGE_SIZE;
	exp_info.flags = O_CLOEXEC | O_RDWR;
	exp_info.priv = imp;
	exp_info.exp_name = "vhost-vdmabuf";

	if (!imp->dma_buf) {
		imp->dma_buf = dma_buf_export(&exp_info);
		if (IS_ERR_OR_NULL(imp->dma_buf)) {
			ret = PTR_ERR(imp->dma_buf);
			imp->dma_buf = NULL;
			goto out;
		}
	}

	log_info("%s[%d] dmabuf: %p\n", __func__, __LINE__, imp->dma_buf);

out:
	return ret;
}

static int vhost_vdmabuf_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int vhost_vdmabuf_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct virtio_vdmabuf_buf *vhost_vdmabuf_dmabuf_import(void *data)
{
	struct virtio_vdmabuf_buf *imp;
	int ret = 0;

	imp = vhost_vdmabuf_get_buf(data);
	if (IS_ERR(imp))
		goto out;

	ret = vhost_vdmabuf_export(imp);
	if (ret) {
		pr_err("%s[%d]failed to dmabuf export. ret %d\n", __func__,
		       __LINE__, ret);
		goto fail_import;
	}

	imp->imported = true;

out:
	return imp;

fail_import:
	vhost_vdmabuf_release_buf(&imp->buf_id);
	kfree(imp->priv);
	kfree(imp->pages_info->pages);
	kfree(imp->pages_info);
	kfree(imp);
	return ERR_PTR(ret);
}

/*
 * ioctl - importing vdmabuf from guest OS
 *
 * user parameters:
 *
 *	virtio_vdmabuf_buf_id_t buf_id - vdmabuf ID of imported buffer
 *	int flags - flags
 *	int fd - file handle of	the imported buffer
 *
 */
static int import_ioctl(struct file *filp, void *data)
{
	struct virtio_vdmabuf_buf *imp;
	struct virtio_vdmabuf_import *exp_attr = data;
	struct task_struct *cur_task = current;
	int ret = 0;

	imp = vhost_vdmabuf_dmabuf_import(data);
	if (IS_ERR(imp)) {
		pr_err("%s[%d] get dmabuf fail, ret: %ld\n", __func__, __LINE__,
		       PTR_ERR(imp));
		ret = PTR_ERR(imp);
		goto out;
	}

	exp_attr->fd = dma_buf_fd(imp->dma_buf, exp_attr->flags);
	if (exp_attr->fd < 0) {
		pr_err("%s[%d] failed to get file descriptor, Process ID: %d, Process name: %s, Thread ID: %d\n",
		       __func__, __LINE__,
		       cur_task->tgid, cur_task->comm, cur_task->pid);
		/* If it is the first import, just return ERR
		 * find -> map_pages -> dma_buf_export
		 * All have been successful, just generate fd fail
		 * So, just call dma_buf_put to release.
		 */
		if (!imp->imported)
			dma_buf_put(imp->dma_buf);
		return exp_attr->fd;
	}

	pr_info("%s[%d] import suc, fd: %d, buf_id: 0x%llx-key %x-%x\n",
		__func__, __LINE__, exp_attr->fd, imp->buf_id.id,
		imp->buf_id.rng_key[0], imp->buf_id.rng_key[1]);
	pr_info("%s[%d] process id: %d, process name: %s, thread id: %d\n",
		__func__, __LINE__, cur_task->tgid, cur_task->comm, cur_task->pid);

out:
	return ret;
}

static int release_ioctl(struct file *filp, void *data)
{
	return 0;
}

/* for kernel user */
struct dma_buf *mtk_vdmabuf_vhost_import(struct virtio_vdmabuf_buf_id_t buf_id)
{
	struct virtio_vdmabuf_import kdata;
	struct virtio_vdmabuf_buf *imp;
	struct task_struct *cur_task = current;

	kdata.buf_id = buf_id;
	imp = vhost_vdmabuf_dmabuf_import(&kdata);
	if (IS_ERR(imp)) {
		pr_err("%s[%d] get dmabuf fail, ret: %ld\n", __func__, __LINE__,
		       PTR_ERR(imp));
		return NULL;
	}

	if (imp->imported)
		get_dma_buf(imp->dma_buf);

	pr_info("%s[%d] import suc, buf_id: 0x%llx-key %x-%x\n",
		__func__, __LINE__, imp->buf_id.id,
		imp->buf_id.rng_key[0], imp->buf_id.rng_key[1]);
	pr_info("%s[%d] process id: %d, process name: %s, thread id: %d\n",
		__func__, __LINE__, cur_task->tgid, cur_task->comm, cur_task->pid);

	return imp->dma_buf;
}
EXPORT_SYMBOL(mtk_vdmabuf_vhost_import);

static const struct virtio_vdmabuf_ioctl_desc vhost_vdmabuf_ioctls[] = {
	VIRTIO_VDMABUF_IOCTL_DEF(VIRTIO_VDMABUF_IOCTL_IMPORT, import_ioctl, 0),
	VIRTIO_VDMABUF_IOCTL_DEF(VIRTIO_VDMABUF_IOCTL_RELEASE, release_ioctl, 0),
};

static long vhost_vdmabuf_ioctl(struct file *filp, unsigned int cmd,
				unsigned long param)
{
	const struct virtio_vdmabuf_ioctl_desc *ioctl;
	virtio_vdmabuf_ioctl_t func;
	unsigned int nr;
	int ret;
	void *udata;

	nr = _IOC_NR(cmd);
	if (nr >= ARRAY_SIZE(vhost_vdmabuf_ioctls)) {
		pr_notice("%s[%d] invalid ioctl\n", __func__, __LINE__);
		return -EINVAL;
	}

	ioctl = &vhost_vdmabuf_ioctls[nr];

	func = ioctl->func;

	if (unlikely(!func)) {
		pr_notice("%s[%d] no function\n", __func__, __LINE__);
		return -EINVAL;
	}

	udata = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
	if (!udata)
		return -ENOMEM;

	if (copy_from_user(udata, (void __user *)param,
			   _IOC_SIZE(cmd)) != 0) {
		pr_err("%s[%d] failed to copy args from userspace\n", __func__,
		       __LINE__);
		ret = -EFAULT;
		goto ioctl_error;
	}

	ret = func(filp, udata);

	if (copy_to_user((void __user *)param, udata,
			 _IOC_SIZE(cmd)) != 0) {
		pr_err("%s[%d] failed to copy args back to userspace\n", __func__,
		       __LINE__);
		ret = -EFAULT;
		goto ioctl_error;
	}

ioctl_error:
	kfree(udata);
	return ret;
}

static const struct file_operations vhost_vdmabuf_fops = {
	.owner = THIS_MODULE,
	.open = vhost_vdmabuf_open,
	.release = vhost_vdmabuf_release,
	.unlocked_ioctl = vhost_vdmabuf_ioctl,
};

static struct miscdevice vhost_vdmabuf_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vhost-vdmabuf",
	.fops = &vhost_vdmabuf_fops,
};

int vhost_vdmabuf_dmabuf_init(void)
{
	int ret = 0;

	ret = misc_register(&vhost_vdmabuf_miscdev);
	if (ret) {
		pr_notice("vhost-vdmabuf: driver can't be registered\n");
		return ret;
	}

	dma_coerce_mask_and_coherent(vhost_vdmabuf_miscdev.this_device,
				     DMA_BIT_MASK(64));

	pr_info("%s[%d] done\n", __func__, __LINE__);

	return ret;
}

void vhost_vdmabuf_dmabuf_deinit(void)
{
	misc_deregister(&vhost_vdmabuf_miscdev);
}
