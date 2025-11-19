/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __UAPI_MTK_MMA_IOCTL_H_
#define __UAPI_MTK_MMA_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define MAX_BUF_TAG_SIZE		16

#define MTK_MMA_DEV_NAME		"/dev/mtk_mma"

enum mtk_mma_buf_tag_index {
	MTK_MMA_BUF_TAG_IDX_SVP		= 0,
	MTK_MMA_BUF_TAG_IDX_SVP_2ND	= 1,
	MTK_MMA_BUF_TAG_IDX_SVP_WFD	= 2,
	MTK_MMA_BUF_TAG_IDX_NR,
};

struct mtk_mma_authorize_data {
	__s64		fd;
	__u64		mma_handle;	/* paddr */
	__u64		size;
	__u32		pipeline_id;
	__s32		buf_tag_idx;
};

struct mtk_mma_get_pipelineid_data {
	__s64		fd;
	__u32		pipeline_id;
	__s32		buf_tag_idx;
};

#define MTK_MMA_IOC_MAGIC		'M'
#define MTK_MMA_IOC_ALLOCPIPELINEID	\
		_IOWR(MTK_MMA_IOC_MAGIC, 5, unsigned int)
#define MTK_MMA_IOC_FREEPIPELINEID	\
		_IOWR(MTK_MMA_IOC_MAGIC, 6, unsigned int)
#define MTK_MMA_IOC_AUTHORIZE		\
		_IOWR(MTK_MMA_IOC_MAGIC, 7, struct mtk_mma_authorize_data)
#define MTK_MMA_IOC_UNAUTHORIZE		\
		_IOWR(MTK_MMA_IOC_MAGIC, 8, struct mtk_mma_authorize_data)
#define MTK_MMA_IOC_GETPIPELINEID	\
		_IOWR(MTK_MMA_IOC_MAGIC, 9, struct mtk_mma_get_pipelineid_data)

#define MTK_MMA_IOC_TEST		\
		_IOWR(MTK_MMA_IOC_MAGIC, 13, struct mtk_mma_authorize_data)

#endif	/* __UAPI_MTK_MMA_IOCTL_H_ */
