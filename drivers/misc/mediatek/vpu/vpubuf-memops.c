// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>

#include "vpubuf-core.h"
#include "vpubuf-memops.h"
static void vpu_common_vm_open(struct vm_area_struct *vma)
{
	struct vpu_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, atomic_read(h->refcount), vma->vm_start,
		 vma->vm_end);

	atomic_inc(h->refcount);
}

/**
 * vpu_common_vm_close() - decrease refcount of the vma
 * @vma:	virtual memory region for the mapping
 *
 * This function releases the user from the provided vma. It expects
 * struct vpu_vmarea_handler pointer in vma->vm_private_data.
 */
static void vpu_common_vm_close(struct vm_area_struct *vma)
{
	struct vpu_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
		 __func__, h, atomic_read(h->refcount), vma->vm_start,
		 vma->vm_end);

	h->put(h->arg);
}

/**
 * vpu_common_vm_ops - common vm_ops used for tracking refcount of mmaped
 * video buffers
 */
const struct vm_operations_struct vpu_common_vm_ops = {
	.open = vpu_common_vm_open,
	.close = vpu_common_vm_close,
};
