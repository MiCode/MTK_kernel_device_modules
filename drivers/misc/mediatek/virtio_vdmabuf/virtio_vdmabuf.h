/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _LINUX_VIRTIO_VDMABUF_H
#define _LINUX_VIRTIO_VDMABUF_H

#include <linux/hashtable.h>
#include <linux/kvm_types.h>
#include <linux/virtio_vdmabuf.h>

struct virtio_vdmabuf_shared_pages {
	/* cross-VM ref addr for the buffer */
	gpa_t ref;

	/* page array */
	struct page **pages;
	gpa_t **l2refs;
	gpa_t *l3refs;

	/* data offset in the first page
	 * and data length in the last page
	 */
	int first_ofst;
	int last_len;

	/* number of shared pages */
	int nents;
};

struct virtio_vdmabuf_buf {
	struct virtio_vdmabuf_buf_id_t buf_id;

	struct dma_buf_attachment *attach;
	struct dma_buf_attachment *d_attach; /* attach to dmabuf heap.*/
	struct dma_buf *dma_buf;
	struct dma_buf *d_dmabuf;
	struct sg_table *sgt;
	struct sg_table *d_sgt; /* sgt to dmabuf heap. */
	struct virtio_vdmabuf_shared_pages *pages_info;
	int vmid;
	int fd;
	uint64_t size;

	/* validity of the buffer */
	bool valid;

	/* set if the buffer is imported via import_ioctl */
	bool imported;

	/* The buffer is not allocated here. It is from dmabuf heap. */
	bool is_dmabuf_heap_buf;

	/* size of private */
	size_t sz_priv;
	/* private data associated with the exported buffer */
	void *priv;

	struct file *filp;
	struct hlist_node node;
};

struct virtio_vdmabuf_event {
	struct virtio_vdmabuf_e_data e_data;
	struct list_head link;
};

struct virtio_vdmabuf_event_queue {
	wait_queue_head_t e_wait;
	struct list_head e_list;

	spinlock_t e_lock;
	struct mutex e_readlock;

	/* # of pending events */
	int pending;
};

/* driver information */
struct virtio_vdmabuf_info {
	struct device *dev;

	struct list_head head_vdmabuf_list;
	struct list_head kvm_instances;

	DECLARE_HASHTABLE(buf_list, 7);

	void *priv;
	struct mutex g_mutex;
	struct notifier_block kvm_notifier;
};

/* IOCTL definitions
 */
typedef int (*virtio_vdmabuf_ioctl_t)(struct file *filp, void *data);

struct virtio_vdmabuf_ioctl_desc {
	unsigned int cmd;
	int flags;
	virtio_vdmabuf_ioctl_t func;
	const char *name;
};

#define VIRTIO_VDMABUF_IOCTL_DEF(ioctl, _func, _flags)	\
	[_IOC_NR(ioctl)] = {			\
			.cmd = ioctl,		\
			.func = _func,		\
			.flags = _flags,	\
			.name = #ioctl		\
}

#define VIRTIO_VDMABUF_VMID(buf_id) ((((buf_id).id) >> 32) & 0xFFFFFFFF)

/* Messages between Host and Guest */

/* List of commands from Guest to Host:
 *
 * ------------------------------------------------------------------
 * A. NEED_VMID
 *
 *  guest asks the host to provide its vmid
 *
 * req:
 *
 * cmd: VIRTIO_VDMABUF_NEED_VMID
 *
 * ack:
 *
 * cmd: same as req
 * op[0] : vmid of guest
 *
 * ------------------------------------------------------------------
 * B. EXPORT
 *
 *  export dmabuf to host
 *
 * req:
 *
 * cmd: VIRTIO_VDMABUF_CMD_EXPORT
 * op0~op3 : HDMABUF ID
 * op4 : number of pages to be shared
 * op5 : offset of data in the first page
 * op6 : length of data in the last page
 * op7 : upper 32 bit of top-level ref of shared buf
 * op8 : lower 32 bit of top-level ref of shared buf
 * op9 : size of private data
 * op10 ~ op64: User private date associated with the buffer
 *	        (e.g. graphic buffer's meta info)
 *
 * ------------------------------------------------------------------
 *
 * List of commands from Host to Guest
 *
 * ------------------------------------------------------------------
 * A. RELEASE
 *
 *  notifying guest that the shared buffer is released by an importer
 *
 * req:
 *
 * cmd: VIRTIO_VDMABUF_CMD_DMABUF_REL
 * op0~op3 : VDMABUF ID
 *
 * ------------------------------------------------------------------
 */

/* msg structures */
struct virtio_vdmabuf_msg {
	struct list_head list;
	unsigned int cmd;
	unsigned int op[64];
};

enum {
	VDMABUF_VQ_RECV = 0,
	VDMABUF_VQ_SEND = 1,
	VDMABUF_VQ_MAX  = 2,
};

enum virtio_vdmabuf_cmd {
	VIRTIO_VDMABUF_CMD_NEED_VMID = 0x10, /* Avoid zero command. */
	VIRTIO_VDMABUF_CMD_EXPORT    = 0x11,
	VIRTIO_VDMABUF_CMD_DMABUF_REL= 0x12,
};

enum virtio_vdmabuf_ops {
	VIRTIO_VDMABUF_HDMABUF_ID_ID = 0,
	VIRTIO_VDMABUF_HDMABUF_ID_RNG_KEY0,
	VIRTIO_VDMABUF_HDMABUF_ID_RNG_KEY1,
	VIRTIO_VDMABUF_NUM_PAGES_SHARED = 4,
	VIRTIO_VDMABUF_FIRST_PAGE_DATA_OFFSET,
	VIRTIO_VDMABUF_LAST_PAGE_DATA_LENGTH,
	VIRTIO_VDMABUF_REF_ADDR_UPPER_32BIT,
	VIRTIO_VDMABUF_REF_ADDR_LOWER_32BIT,
	VIRTIO_VDMABUF_PRIVATE_DATA_SIZE,
	VIRTIO_VDMABUF_PRIVATE_DATA_START
};

/* adding exported/imported vdmabuf info to hash */
static inline int
virtio_vdmabuf_add_buf(struct virtio_vdmabuf_info *info,
		       struct virtio_vdmabuf_buf *new)
{
	hash_add(info->buf_list, &new->node, new->buf_id.id);
	return 0;
}

/* comparing two vdmabuf IDs */
static inline bool
is_same_buf(struct virtio_vdmabuf_buf_id_t a,
	    struct virtio_vdmabuf_buf_id_t b)
{
	int i;

	if (a.id != b.id)
		return false;

	/* compare keys */
	for (i = 0; i < 2; i++) {
		if (a.rng_key[i] != b.rng_key[i])
			return false;
	}

	return true;
}

/* find buf for given vdmabuf ID */
static inline struct virtio_vdmabuf_buf
*virtio_vdmabuf_find_buf(struct virtio_vdmabuf_info *info,
			 struct virtio_vdmabuf_buf_id_t *buf_id)
{
	struct virtio_vdmabuf_buf *found;

	hash_for_each_possible(info->buf_list, found, node, buf_id->id)
		if (is_same_buf(found->buf_id, *buf_id))
			return found;

	return NULL;
}

/* find buf for given fd */
static inline struct virtio_vdmabuf_buf
*virtio_vdmabuf_find_buf_fd(struct virtio_vdmabuf_info *info, int fd)
{
	struct virtio_vdmabuf_buf *found;
	int i;

	hash_for_each(info->buf_list, i, found, node)
		if (found->fd == fd)
			return found;

	return NULL;
}

/* delete buf from hash */
static inline int
virtio_vdmabuf_del_buf(struct virtio_vdmabuf_info *info,
		       struct virtio_vdmabuf_buf_id_t *buf_id)
{
	struct virtio_vdmabuf_buf *found;

	found = virtio_vdmabuf_find_buf(info, buf_id);
	if (!found)
		return -ENOENT;

	hash_del(&found->node);

	return 0;
}

#endif
