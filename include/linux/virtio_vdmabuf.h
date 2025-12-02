/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _UAPI_LINUX_VIRTIO_VDMABUF_H
#define _UAPI_LINUX_VIRTIO_VDMABUF_H

#define MAX_SIZE_PRIV_DATA 192

typedef unsigned long long __u64;
typedef unsigned int uint32_t;

struct virtio_vdmabuf_buf_id_t{
	__u64 id;
	/* 8B long Random number */
	int rng_key[2];
};

struct virtio_vdmabuf_e_hdr {
	/* buf_id of new buf */
	struct virtio_vdmabuf_buf_id_t buf_id;
	/* size of private data */
	int size;
};

struct virtio_vdmabuf_e_data {
	struct virtio_vdmabuf_e_hdr hdr;
	/* ptr to private data */
	void /* _user */ *data;
};

#define VIRTIO_VDMABUF_IOCTL_IMPORT \
_IOC(_IOC_NONE, 'G', 2, sizeof(struct virtio_vdmabuf_import))
#define VIRTIO_VDMABUF_IOCTL_RELEASE \
_IOC(_IOC_NONE, 'G', 3, sizeof(struct virtio_vdmabuf_import))
struct virtio_vdmabuf_import {
	/* IN parameters */
	/* vdmabuf id to be imported */
	struct virtio_vdmabuf_buf_id_t buf_id;
	/* flags */
	int flags;
	/* OUT parameters */
	/* exported dma buf fd */
	int fd;
};

#define VIRTIO_VDMABUF_IOCTL_EXPORT \
_IOC(_IOC_NONE, 'G', 4, sizeof(struct virtio_vdmabuf_export))
struct virtio_vdmabuf_export {
	/* IN parameters */
	/* DMA buf fd to be exported */
	int fd;
	/* exported dma buf id */
	struct virtio_vdmabuf_buf_id_t buf_id;
	int sz_priv;
	char *priv;
};

#define VIRTIO_VDMABUF_IOCTL_ALLOC_FD \
_IOC(_IOC_NONE, 'G', 5, sizeof(struct virtio_vdmabuf_alloc))
struct virtio_vdmabuf_alloc {
	/* IN parameters */
	uint32_t size;
	/* OUT parameters */
	int fd;
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_VIRTIO_VDMABUF)
/*
 * for kernel user, only need to provide dmabuf and we will return buf_id.
 * A buf_id will be returned so that yocto can get this dmabuf.
 */
int mtk_vdmabuf_vguest_export(struct dma_buf *dmabuf,
			      struct virtio_vdmabuf_buf_id_t *buf_id);
/* for vdmabuf heap debug */
bool is_shared_dmabuf_by_vdmabuf(struct device *dev);
void virtio_vdmabuf_heap_debug_attach_dump(struct seq_file *s, int *attach_cnt,
					   struct dma_buf_attachment *attach);
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_VHOST_VDMABUF)
/*
 * for kernel user, only need to provide bufid get dmabuf of android share
 */
struct dma_buf *mtk_vdmabuf_vhost_import(struct virtio_vdmabuf_buf_id_t buf_id);
#endif

#endif
