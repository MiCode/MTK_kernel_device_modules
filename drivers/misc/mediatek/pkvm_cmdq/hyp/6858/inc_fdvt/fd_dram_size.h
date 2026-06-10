/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#define fdvt_fdmode_rs_confi_size 224
#define fdvt_fdmode_fd_confi_size 18432
#define fdvt_fdmode_yuv2rgb_confi_size 112
#define fdvt_fdmode_fd_pose_confi_size 576

#define fdvt_attrmode_rs_confi_size 0
#define fdvt_attrmode_fd_confi_size 4992
#define fdvt_attrmode_yuv2rgb_confi_size 112

#define fdvt_fd_result_size 49152 // 384 * 1024 / 8
#define fdvt_fd_loop_num 96
#define fdvt_fd_rpn0_loop_num 95
#define fdvt_fd_rpn1_loop_num 63
#define fdvt_fd_rpn2_loop_num 31

#define fdvt_fd_pyramid0_start_loop 64
#define fdvt_fd_pyramid1_start_loop 32
#define fdvt_fd_pyramid2_start_loop 0

#define fdvt_attr_loop_num 26
#define fdvt_age_output_regression 17
#define fdvt_gender_output_regression 20
#define fdvt_isIndian_output_regression 22
#define fdvt_race_output_regression 25

#define input_WDMA_WRA_num 4
#define output_WDMA_WRA_num 4
#define kernel_RDMA_RA_num 2

#define MAX_ENQUE_FRAME_NUM 10

#define ATTR_MODE_PYRAMID_WIDTH 128

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
