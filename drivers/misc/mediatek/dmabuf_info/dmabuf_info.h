/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _DMABUF_INFO_H_
#define _DMABUF_INFO_H_


struct dma_mem_entry {
	uint64_t addr;
	uint32_t length;
	uint32_t padding;
};

struct dmabuf_info {
	__u32 nr_entries;
	struct dma_mem_entry *entries;
	int fd;
};


#define DEVICE_NAME "dmabuf_info"

#define DMABUF_INFO_IOCTL_CMD_GET_ENTRY_NUM	_IOR('M', 1, struct dmabuf_info)
#define DMABUF_INFO_IOCTL_CMD_GET_ALL		_IOR('M', 2, struct dmabuf_info)
#define DMABUF_INFO_IOCTL_CMD_GET_HW_CLK	_IOR('M', 3, uint64_t)

#endif
