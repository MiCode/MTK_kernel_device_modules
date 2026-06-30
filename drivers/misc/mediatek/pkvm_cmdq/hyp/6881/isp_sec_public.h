/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef _ISP_SEC_PLATFORM_H_
#define _ISP_SEC_PLATFORM_H_

#include <asm/kvm_pkvm_module.h>
#include "cmdq_sec_iwc_common.h"

struct isp_meta_cq {
	uint64_t CqSecPA;
	uint64_t TpipeSecPA;
	uint64_t CqSecMVA;
	uint64_t BpciSecPA;
	uint64_t LsciSecPA;
	uint64_t LceiSecPA;
	uint64_t DepiSecPA;
	uint64_t DmgiSecPA;
};

struct isp_meta_fd {
	uint64_t ImgSrcY_PA;
	uint64_t ImgSrcUV_PA;
	uint64_t YUVConfig_PA;
	uint64_t YUVOutBuf_PA;
	uint64_t RSConfig_PA;
	uint64_t RSOutBuf_PA;
	uint64_t FDConfig_PA;
	uint64_t FD_Pose_Config_PA;
	uint64_t FDOutBuf_PA;
	uint64_t FDResultBuf_PA;
	uint64_t LearningData_PA[18];
	uint64_t ExtraLearningData_PA[18];
	uint32_t ImgSrcY_MVA;
	uint32_t ImgSrcUV_MVA;
	uint32_t YUVConfig_MVA;
	uint32_t YUVOutBuf_MVA;
	uint32_t RSConfig_MVA;
	uint32_t RSOutBuf_MVA;
	uint32_t FDConfig_MVA;
	uint32_t FD_Pose_Config_MVA;
	uint32_t FDOutBuf_MVA;
	uint32_t FDResultBuf_MVA;
	uint32_t LearningData_MVA[18];
	uint32_t ExtraLearningData_MVA[18];
	uint32_t ImgSrcY_VA;
	uint32_t ImgSrcUV_VA;
	uint32_t YUVConfig_VA;
	uint32_t YUVOutBuf_VA;
	uint32_t RSConfig_VA;
	uint32_t RSOutBuf_VA;
	uint32_t FDConfig_VA;
	uint32_t FD_Pose_Config_VA;
	uint32_t FDOutBuf_VA;
	uint64_t FDResultBuf_VA;
	uint32_t LearningData_VA[18];
	uint32_t ExtraLearningData_VA[18];
};

struct isp_exec_metadata {
	union {
		struct isp_meta_cq cq;
		struct isp_meta_fd fd;
	};
};

struct FDVT_ROI {
	uint32_t x1;
	uint32_t y1;
	uint32_t x2;
	uint32_t y2;
};

struct FDVT_PADDING {
	uint32_t left;
	uint32_t right;
	uint32_t down;
	uint32_t up;
};
struct FDVT_MetaDataToGCE {
	unsigned int ImgSrcY_Handler;
	unsigned int ImgSrcUV_Handler;
	unsigned int YUVConfig_Handler;
	unsigned int YUVOutBuf_Handler;
	unsigned int RSConfig_Handler;
	unsigned int RSOutBuf_Handler;
	unsigned int FDConfig_Handler;
	unsigned int FDOutBuf_Handler;
	unsigned int FD_POSE_Config_Handler;
	unsigned int FDResultBuf_MVA;
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

struct FDVTSecureMeta {
	unsigned int Learning_Type;
	unsigned int fd_mode;
	unsigned int source_img_width[15];
	unsigned int source_img_height[15];
	unsigned int RIP_feature;
	unsigned int GFD_skip;
	unsigned int GFD_skip_V;
	unsigned int feature_threshold;
	unsigned int source_img_fmt;
	bool scale_from_original;
	bool scale_manual_mode;
	unsigned int scale_num_from_user;
	bool dynamic_change_model[18];
	unsigned int ImgSrcY_Handler;
	unsigned int ImgSrcUV_Handler;
	unsigned int RSConfig_Handler;
	unsigned int RSOutBuf_Handler;
	unsigned int FDConfig_Handler;
	uint64_t     FDResultBuf_PA;
	unsigned int Learning_Data_Handler[18];
	unsigned int Extra_Learning_Data_Handler[18];
	unsigned int ImgSrc_Y_Size;
	unsigned int ImgSrc_UV_Size;
	unsigned int RSConfigSize;
	unsigned int RSOutBufSize;
	unsigned int FDConfigSize;
	unsigned int FDResultBufSize;
	unsigned int Learning_Data_Size[18];
	unsigned short SecMemType;
	bool CarvedOutResult;
	bool isReleased;
};

int32_t cmdq_drv_isp_setup_task_fd(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus);

int32_t cmdq_drv_isp_setup_iova(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus);

int32_t cmdq_drv_isp_setup_task_cq(
	struct iwc_cq_meta *msgex,
	struct iwc_cq_meta2 *msgex2,
	struct isp_meta_cq *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus);

uint32_t isp_secio_read(const uint32_t addr, int dip_case);

void cmdq_drv_isp_dump_task_cq(void);

int32_t cmdq_drv_imgsys_set_domain(void *data, bool isSet);
int32_t cmdq_drv_imgsys_set_slc(void *data);
void cmdq_drv_imgsys_slc_cb(const uint32_t base);

#endif
