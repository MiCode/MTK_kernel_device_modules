/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _MT_SEC_FDVT_H
#define _MT_SEC_FDVT_H

#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_COMPAT)
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

struct imem_buf_info {
	void *va;
	u64 iova;
	struct iosys_map map;
	unsigned int size;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	uint64_t pa;
};

struct FDVT_ONETIME_MEM_RECORD {
	struct imem_buf_info YUVConfig;
	struct imem_buf_info RSConfig;
	struct imem_buf_info RSOutBuf;
	struct imem_buf_info FDConfig;
	struct imem_buf_info FDOutBuf;
	struct imem_buf_info FD_POSE;
	struct imem_buf_info FDResultBuf_MVA;
	u64 ImgY_Handler;
	u64 ImgUV_Handler;
	u64 YUVConfig_Handler;
	u64 RSConfig_Handler;
	u64 RSOutBuf_Handler;
	u64 FDConfig_Handler;
	u64 FDOutBuf_Handler;
	u64 FD_POSE_Config_Handler;
	unsigned int handler_first_time;
	unsigned int iova_first_time;
	unsigned int tzmp1_first_time;
};

struct FDVT_MEM_RECORD {
	struct imem_buf_info ImgSrcY;
	struct imem_buf_info ImgSrcUV;
};

struct FDVT_SEC_MetaDataToGCE {
	u64 ImgSrcY_Handler; //Handler -> VA for UT
	u64 ImgSrcUV_Handler; //Handler -> VA for UT
	u64 YUVConfig_Handler; //get VA
	u64 RSConfig_Handler; //get VA
	u64 RSOutBuf_Handler;
	u64 FDConfig_Handler; //get VA
	u64 FDOutBuf_Handler;
	u64 FD_POSE_Config_Handler; //get VA
	u64 ImgSrcY_IOVA;
	u64 ImgSrcUV_IOVA;
	u64 YUVConfig_IOVA;
	u64 YUVOutBuf_IOVA;
	u64 RSConfig_IOVA;
	u64 RSOutBuf_IOVA;
	u64 FDConfig_IOVA;
	u64 FDOutBuf_IOVA;
	u64 FDPOSE_IOVA;
	u64 FD_POSE_Config_IOVA;
	u64 FDResultBuf_MVA;
	unsigned int ImgSrc_Y_Size;
	unsigned int ImgSrc_UV_Size;
	unsigned int YUVConfigSize;
	unsigned int YUVOutBufSize;
	unsigned int RSConfigSize;
	unsigned int RSOutBufSize;
	unsigned int FDConfigSize;
	unsigned int FD_POSE_ConfigSize;
	unsigned int FDOutBufSize;
	unsigned int FDResultBufSize;
	unsigned int FDMode;
	unsigned int srcImgFmt;
	unsigned int srcImgWidth;
	unsigned int srcImgHeight;
	unsigned int maxWidth;
	unsigned int maxHeight;
	unsigned int rotateDegree;
	unsigned short featureTH;
	unsigned short SecMemType;
	unsigned int enROI;
	struct FDVT_ROI src_roi;
	unsigned int enPadding;
	struct FDVT_PADDING src_padding;
	unsigned int SRC_IMG_STRIDE;
	unsigned int pyramid_width;
	unsigned int pyramid_height;
	bool isReleased;
};

#endif
