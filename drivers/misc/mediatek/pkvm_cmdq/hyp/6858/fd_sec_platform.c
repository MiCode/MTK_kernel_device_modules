// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include "pkvm_cmdq_platform.h"
#include "pkvm_cmdq_hyp.h"
#include "isp_sec_public.h"
#include <linux/slab.h>
#include <asm/kvm_pkvm_module.h>


const struct pkvm_module_ops *pkvm_cmdq_fdvt_ops;
#define CALL_FROM_CMDQ_FDVT_OPS(fn, ...) pkvm_cmdq_fdvt_ops->fn(__VA_ARGS__)

#ifdef memcpy
#undef memcpy
#endif

#ifdef memset
#undef memset
#endif

#include "all_header.h"
#include "fd_dram_size.h"
#include "fd_dram_info.h"
// #define DRIVER_UT

#ifdef DRIVER_UT
#include "fd_in1_0910_338x600_FMT_YVU_2P.h"
#include "fd_in2_0910_338x600_FMT_YVU_2P.h"
#endif

#define NEW_BUFFER
#define MODIFIED
#define OUTPUT_USE_NORMAL_BUFFER

enum FDVTMODE {
	FDMODE = 0,
	ATTRIBUTEMODE = 1,
	POSEMODE = 2
};

enum FDVTFORMAT {
	FMT_NA = 0,
	FMT_YUV_2P = 1,
	FMT_YVU_2P = 2,
	FMT_YUYV = 3, //1 plane
	FMT_YVYU = 4, //1 plane
	FMT_UYVY = 5, //1 plane
	FMT_VYUY = 6, //1 plane
	FMT_MONO = 7         // AIE2.0
};

enum FDVTINPUTDEGREE {
	DEGREE_0 = 0,
	DEGREE_90 = 1,
	DEGREE_270 = 2,
	DEGREE_180 = 3
};

struct FdDrv_Para {
	uint16_t FD_MODE;
	uint16_t MAX_SRC_Input_Width;
	uint16_t MAX_SRC_Input_Height;
	uint16_t SRC_Input_Width;
	uint16_t SRC_Input_Height;
	uint16_t SRC_Crop_Width;
	uint16_t SRC_Crop_Height;
	uint16_t SRC_IMG_FMT;
	uint16_t INPUT_ROTATE_DEGREE;
	signed short RPN_ANCHOR_THRD;
	uint16_t pyramid_width;
	uint16_t pyramid_height;
	//int RPN_ANCHOR_THRD;
	//uint64_t *source_img_address;
	//uint64_t *source_img_address_UV;
	uint64_t source_img_address;
	uint64_t source_img_address_UV;

	uint64_t *FDMODE_FD_Config_VA;
	uint64_t *FDMODE_RS_Config_VA;
	uint64_t *FDMODE_YUV2RGB_Config_VA;
	uint64_t *FDMODE_FD_POSE_Config_VA;

	uint64_t *ATTRMODE_FD_Config_VA[MAX_ENQUE_FRAME_NUM];
	uint64_t *ATTRMODE_YUV2RGB_Config_VA[MAX_ENQUE_FRAME_NUM];

	uint64_t *RS_Pyramid0_R_Result_VA;
	uint64_t *RS_Pyramid0_G_Result_VA;
	uint64_t *RS_Pyramid0_B_Result_VA;
	uint64_t *RS_Pyramid1_R_Result_VA;
	uint64_t *RS_Pyramid1_G_Result_VA;
	uint64_t *RS_Pyramid1_B_Result_VA;
	uint64_t *RS_Pyramid2_R_Result_VA;
	uint64_t *RS_Pyramid2_G_Result_VA;
	uint64_t *RS_Pyramid2_B_Result_VA;

	uint32_t *FDMODE_FD_Config_PA;
	uint32_t *FDMODE_RS_Config_PA;
	uint32_t *FDMODE_YUV2RGB_Config_PA;
	uint32_t *FDMODE_FD_POSE_Config_PA;

	uint32_t *ATTRMODE_FD_Config_PA[MAX_ENQUE_FRAME_NUM];
	uint32_t *ATTRMODE_YUV2RGB_Config_PA[MAX_ENQUE_FRAME_NUM];

	uint32_t *RS_Pyramid0_R_Result_PA;
	uint32_t *RS_Pyramid0_G_Result_PA;
	uint32_t *RS_Pyramid0_B_Result_PA;
	uint32_t *RS_Pyramid1_R_Result_PA;
	uint32_t *RS_Pyramid1_G_Result_PA;
	uint32_t *RS_Pyramid1_B_Result_PA;
	uint32_t *RS_Pyramid2_R_Result_PA;
	uint32_t *RS_Pyramid2_G_Result_PA;
	uint32_t *RS_Pyramid2_B_Result_PA;
};

struct FdDrv_Attr_Para {
	uint32_t WriteIdx;
	uint32_t ReadIdx;
	uint16_t FD_MODE[MAX_ENQUE_FRAME_NUM];
	uint16_t SRC_Input_Width[MAX_ENQUE_FRAME_NUM];
	uint16_t SRC_Input_Height[MAX_ENQUE_FRAME_NUM];
	uint16_t SRC_Crop_Width[MAX_ENQUE_FRAME_NUM];
	uint16_t SRC_Crop_Height[MAX_ENQUE_FRAME_NUM];
	uint16_t SRC_IMG_FMT[MAX_ENQUE_FRAME_NUM];
	uint16_t INPUT_ROTATE_DEGREE[MAX_ENQUE_FRAME_NUM];
	uint64_t source_img_address[MAX_ENQUE_FRAME_NUM];
	uint64_t source_img_address_UV[MAX_ENQUE_FRAME_NUM];
};

struct FdDrv_FD_DMA_Para {
	uint64_t *fd_out_hw_PKVM_PA[fdvt_fd_loop_num][output_WDMA_WRA_num];
	uint64_t *fd_kernel_PKVM_PA[fdvt_fd_loop_num][kernel_RDMA_RA_num];
	uint64_t *attr_out_hw_VA[fdvt_attr_loop_num][output_WDMA_WRA_num];
	uint64_t *attr_kernel_VA[fdvt_attr_loop_num][kernel_RDMA_RA_num];

	uint64_t *age_out_hw_VA[MAX_ENQUE_FRAME_NUM];
	uint64_t *gender_out_hw_VA[MAX_ENQUE_FRAME_NUM];
	uint64_t *isIndian_out_hw_VA[MAX_ENQUE_FRAME_NUM];
	uint64_t *race_out_hw_VA[MAX_ENQUE_FRAME_NUM];

	uint64_t *fd_pose_out_hw_VA[3][output_WDMA_WRA_num];

	uint32_t *fd_out_hw_PA[fdvt_fd_loop_num][output_WDMA_WRA_num];
	uint32_t *fd_kernel_PA[fdvt_fd_loop_num][kernel_RDMA_RA_num];
	uint32_t *attr_out_hw_PA[fdvt_attr_loop_num][output_WDMA_WRA_num];
	uint32_t *attr_kernel_PA[fdvt_attr_loop_num][kernel_RDMA_RA_num];

	uint32_t *age_out_hw_PA[MAX_ENQUE_FRAME_NUM];
	uint32_t *gender_out_hw_PA[MAX_ENQUE_FRAME_NUM];
	uint32_t *isIndian_out_hw_PA[MAX_ENQUE_FRAME_NUM];
	uint32_t *race_out_hw_PA[MAX_ENQUE_FRAME_NUM];

	uint32_t *fd_pose_out_hw_PA[3][output_WDMA_WRA_num];
};

static struct FdDrv_Para Fd_Para;
static struct FdDrv_FD_DMA_Para Fd_FD_DMA_Para;
static uint32_t g_RSConfig_IOVA;
static uint32_t g_FDConfig_IOVA;
static uint32_t g_YUVConfig_IOVA;
static uint32_t g_FDPOSE_IOVA;
struct FdDrv_Para *g_FdDrv_Para;
struct FdDrv_FD_DMA_Para *g_FdDrv_Fd_DMA_Para;
uint32_t fdvt_frame_R_size;
uint32_t fdvt_frame_G_size;
uint32_t fdvt_frame_B_size;
uint32_t fdvt_rs_pyramid0_out_size;
uint32_t fdvt_rs_pyramid1_out_size;
uint32_t fdvt_rs_pyramid2_out_size;
uint32_t g_FD_Config[FD_CONFIG_SIZE * fdvt_fd_loop_num];
unsigned int gFD_pose_height;

void Get_RSConfig_IOVA(uint32_t *iova)
{
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Get RSConfig IOVA\n");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_RSConfig_IOVA);
	*iova = g_RSConfig_IOVA;
}

void Get_FDConfig_IOVA(uint32_t *iova)
{
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Get FDConfig IOVA\n");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_FDConfig_IOVA);
	*iova = g_FDConfig_IOVA;
}

void Get_YUVConfig_IOVA(uint32_t *iova)
{
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Get YUVConfig IOVA\n");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_YUVConfig_IOVA);
	*iova = g_YUVConfig_IOVA;
}

void Get_FDPOSE_IOVA(uint32_t *iova)
{
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Get FDPOSE IOVA\n");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_FDPOSE_IOVA);
	*iova = g_FDPOSE_IOVA;
}

void FDVT_initFdvtTable(uint16_t pyramid_width, uint16_t pyramid_height)
{
	int i = 0;

	image_width[fdvt_fd_pyramid2_start_loop] = pyramid_width / 4;
	image_height[fdvt_fd_pyramid2_start_loop] = pyramid_height / 4;

	image_width[fdvt_fd_pyramid1_start_loop] = pyramid_width / 2;
	image_height[fdvt_fd_pyramid1_start_loop] = pyramid_height / 2;

	image_width[fdvt_fd_pyramid0_start_loop] = pyramid_width;
	image_height[fdvt_fd_pyramid0_start_loop] = pyramid_height;

	for (i = 0; i < fdvt_fd_loop_num; i++) {
		if (i != fdvt_fd_pyramid2_start_loop && i != fdvt_fd_pyramid1_start_loop && i
		!= fdvt_fd_pyramid0_start_loop) {
			if (Used_Output_Stride2_as_Input[i] == 1) {
				image_width[i] = stride2_output_width[i - 1];
				image_height[i] = stride2_output_height[i - 1];
			} else {
				image_width[i] = output_width[i - 1];
				image_height[i] = output_height[i - 1];
			}
		}

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1) {
			output_width[i] = (image_width[i] - 1) / (2 * fd_maxpool[i]) + 1;
			output_height[i] = (image_height[i] - 1) / (2 * fd_maxpool[i]) + 1;
		} else {
			output_width[i] = (image_width[i] - 1) / (fd_stride[i] + 2 * fd_maxpool[i])
			+ 1;
			output_height[i] = (image_height[i] - 1) / (fd_stride[i] + 2 * fd_maxpool[i]
			) + 1;
		}

		stride2_output_width[i] = ((output_width[i] - 1) / 2 + 1) * out_2size[i];
		stride2_output_height[i] = ((output_height[i] - 1) / 2 + 1) * out_2size[i];

		out_xsize_plus_1[i] = output_width[i] * output_channel_pack[i] *
		((outlayer[i] == 1) ? 2 : 1);
		out_stride[i] = ((((outlayer[i] == 1) ? out_xsize_plus_1[i] *
		anchor_enable_number[i] : out_xsize_plus_1[i]) - 1) / 16 + 1) * 16;

		out_xsize_plus_1_stride2[i] = ((output_width[i] - 1) / 2 + 1) *
		output_channel_pack[i] * ((outlayer[i] == 1) ? 2 : 1) * out_2size[i];
		out_stride_stride2[i] = ((out_xsize_plus_1_stride2[i] - 1) / 16 + 1) * 16;

		out_ysize_plus_1_stride2[i] = (out_2size[i] == 1 ? (output_height[i] - 1) / 2
		+ 1: output_height[i]);

		if (output_WDMA_WRA_buffer_en[i][0]) {
			if (i == fdvt_fd_rpn2_loop_num || i == fdvt_fd_rpn1_loop_num || i ==
			fdvt_fd_rpn0_loop_num) {
				output_WDMA_WRA_buffer_size[i][0] = fdvt_fd_result_size;
			} else {
				output_WDMA_WRA_buffer_size[i][0] = output_height[i] * out_stride[i];
			}
		}

		if (outlayer[i] == 1) {
			if (output_WDMA_WRA_buffer_en[i][1])
				output_WDMA_WRA_buffer_size[i][1] = output_WDMA_WRA_buffer_size[i][0];
			if (output_WDMA_WRA_buffer_en[i][2])
				output_WDMA_WRA_buffer_size[i][2] = output_WDMA_WRA_buffer_size[i][0];
			if (output_WDMA_WRA_buffer_en[i][3])
				output_WDMA_WRA_buffer_size[i][3] = output_WDMA_WRA_buffer_size[i][0];
		} else if (i == fdvt_fd_rpn2_loop_num || i == fdvt_fd_rpn1_loop_num || i ==
		fdvt_fd_rpn0_loop_num) {
			output_WDMA_WRA_buffer_size[i][0] = fdvt_fd_result_size;
		} else {
			if (output_WDMA_WRA_buffer_en[i][1])
				output_WDMA_WRA_buffer_size[i][1] = output_height[i] * out_stride[i];
			if (output_WDMA_WRA_buffer_en[i][2])
				output_WDMA_WRA_buffer_size[i][2] = out_ysize_plus_1_stride2[i] *
				out_stride_stride2[i];
			if (output_WDMA_WRA_buffer_en[i][3])
				output_WDMA_WRA_buffer_size[i][3] = out_ysize_plus_1_stride2[i] *
				out_stride_stride2[i];
		}
	}

	for (i = 0; i < fdvt_fd_loop_num; i++) {
		if (input_channel_pack[i] == 1)
			input_xsize_plus_1[i] = ((image_width[i] - 1) / 8 + 1) * 8;
		else
			input_xsize_plus_1[i] = image_width[i] * input_channel_pack[i];
	}
}

void FDVT_arrangeConfigAddress_PA(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData,
	struct isp_meta_fd *FDVT_ExecMeta)
{
	//RS DRAM
	if (FDVT_MetaData->FDMode == 0) {
		g_FdDrv_Para->FDMODE_RS_Config_PA = (uint32_t *)(uintptr_t
		)FDVT_MetaData->RSConfig_IOVA;
	}

	//FD DRAM
	if (FDVT_MetaData->FDMode == 0) {
		g_FdDrv_Para->FDMODE_FD_Config_PA = (uint32_t *)(uintptr_t
		)FDVT_MetaData->FDConfig_IOVA;
	}

	//YUV2RGB DRAM
	if (FDVT_MetaData->FDMode == 0) {
		g_FdDrv_Para->FDMODE_YUV2RGB_Config_PA = (uint32_t *)(uintptr_t
		)FDVT_MetaData->YUVConfig_IOVA;
	}

	if (FDVT_MetaData->FDMode == 0) {
		g_FdDrv_Para->FDMODE_FD_POSE_Config_PA = (uint32_t *)(uintptr_t
		)FDVT_MetaData->FD_POSE_Config_IOVA;
	}
}

void FDVT_arrangeOutputAddress_PA(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData,
	struct isp_meta_fd *FDVT_ExecMeta)
{

	g_FdDrv_Para->RS_Pyramid0_R_Result_PA = (uint32_t *)(uintptr_t
	)FDVT_MetaData->RSOutBuf_IOVA;
	g_FdDrv_Para->RS_Pyramid0_G_Result_PA = g_FdDrv_Para->RS_Pyramid0_R_Result_PA
	+ fdvt_rs_pyramid0_out_size/4;
	g_FdDrv_Para->RS_Pyramid0_B_Result_PA = g_FdDrv_Para->RS_Pyramid0_G_Result_PA
	+ fdvt_rs_pyramid0_out_size/4;
	g_FdDrv_Para->RS_Pyramid1_R_Result_PA = g_FdDrv_Para->RS_Pyramid0_B_Result_PA
	+ fdvt_rs_pyramid0_out_size/4;
	g_FdDrv_Para->RS_Pyramid1_G_Result_PA = g_FdDrv_Para->RS_Pyramid1_R_Result_PA
	+ fdvt_rs_pyramid1_out_size/4;
	g_FdDrv_Para->RS_Pyramid1_B_Result_PA = g_FdDrv_Para->RS_Pyramid1_G_Result_PA
	+ fdvt_rs_pyramid1_out_size/4;
	g_FdDrv_Para->RS_Pyramid2_R_Result_PA = g_FdDrv_Para->RS_Pyramid1_B_Result_PA
	+ fdvt_rs_pyramid1_out_size/4;
	g_FdDrv_Para->RS_Pyramid2_G_Result_PA = g_FdDrv_Para->RS_Pyramid2_R_Result_PA
	+ fdvt_rs_pyramid2_out_size/4;
	g_FdDrv_Para->RS_Pyramid2_B_Result_PA = g_FdDrv_Para->RS_Pyramid2_G_Result_PA
	+ fdvt_rs_pyramid2_out_size/4;


	uint32_t *currentPA = NULL, *nextPA = NULL;
	uint32_t *currentResultPA = NULL;
	uint8_t i=0, j=0;

	currentResultPA = (uint32_t *)(uintptr_t)FDVT_MetaData->FDResultBuf_MVA;
	//currentResultVA = (uint64_t *)FDVT_ExecMeta->FDResultBuf_VA;

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[0][0] =
	(uint32_t *)(uintptr_t)FDVT_MetaData->FDOutBuf_IOVA;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[1][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[0][0] + output_WDMA_WRA_buffer_size[0][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[1][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[1][0] + output_WDMA_WRA_buffer_size[1][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[2][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[1][2] + output_WDMA_WRA_buffer_size[1][2] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[2][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[2][0] + output_WDMA_WRA_buffer_size[2][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[3][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[2][2] + output_WDMA_WRA_buffer_size[2][2] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[3][0] + output_WDMA_WRA_buffer_size[3][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][0] + output_WDMA_WRA_buffer_size[4][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][1] + output_WDMA_WRA_buffer_size[4][1] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][2] + output_WDMA_WRA_buffer_size[4][2] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[4][3] + output_WDMA_WRA_buffer_size[4][3] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][0] + output_WDMA_WRA_buffer_size[5][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][1] + output_WDMA_WRA_buffer_size[5][1] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][2] + output_WDMA_WRA_buffer_size[5][2] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[6][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[5][3] + output_WDMA_WRA_buffer_size[5][3] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[7][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[6][0] + output_WDMA_WRA_buffer_size[6][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[7][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[7][0] + output_WDMA_WRA_buffer_size[7][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[8][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[7][2] + output_WDMA_WRA_buffer_size[7][2] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[8][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[8][0] + output_WDMA_WRA_buffer_size[8][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[9][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[8][1] + output_WDMA_WRA_buffer_size[8][1] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[10][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[9][0] + output_WDMA_WRA_buffer_size[9][0] /
	4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[10][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[10][0] + output_WDMA_WRA_buffer_size[10][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[11][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[10][2] + output_WDMA_WRA_buffer_size[10][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[11][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[11][0] + output_WDMA_WRA_buffer_size[11][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[12][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[11][1] + output_WDMA_WRA_buffer_size[11][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[13][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[12][0] + output_WDMA_WRA_buffer_size[12][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[14][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[13][0] + output_WDMA_WRA_buffer_size[13][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[15][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[14][0] + output_WDMA_WRA_buffer_size[14][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[16][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[15][0] + output_WDMA_WRA_buffer_size[15][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[17][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[16][0] + output_WDMA_WRA_buffer_size[16][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[18][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[17][0] + output_WDMA_WRA_buffer_size[17][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[18][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[18][0] + output_WDMA_WRA_buffer_size[18][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[18][1] + output_WDMA_WRA_buffer_size[18][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] + output_WDMA_WRA_buffer_size[19][0]
	/ 4 + output_WDMA_WRA_buffer_size[19][2] / 4 +
	output_WDMA_WRA_buffer_size[20][0] / 4 + output_WDMA_WRA_buffer_size[20][2] /
	4 + output_WDMA_WRA_buffer_size[21][0] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] + 1 * out_xsize_plus_1[19] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1] + 1 * out_xsize_plus_1[19] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[20][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] + 2 * out_xsize_plus_1[20] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[20][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1] + 2 * out_xsize_plus_1[20] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[20][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] + 3 * out_xsize_plus_1[20] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[20][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1] + 3 * out_xsize_plus_1[20] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[21][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0] + 4 * out_xsize_plus_1[21] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[21][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1] + 4 * out_xsize_plus_1[21] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[21][1] + output_WDMA_WRA_buffer_size[21][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0] + 1 * out_xsize_plus_1[22] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[23][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0] + 2 * out_xsize_plus_1[23] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[23][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0] + 3 * out_xsize_plus_1[23] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[24][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0] + 4 * out_xsize_plus_1[24] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[24][0] + output_WDMA_WRA_buffer_size[24][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][0] + 1 * out_xsize_plus_1[25] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[26][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][0] + 2 * out_xsize_plus_1[26] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[26][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][0] + 3 * out_xsize_plus_1[26] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[27][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[25][0] + 4 * out_xsize_plus_1[27] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[27][0] + output_WDMA_WRA_buffer_size[27][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0] + 1 * out_xsize_plus_1[28] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[29][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0] + 2 * out_xsize_plus_1[29] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[29][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0] + 3 * out_xsize_plus_1[29] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[30][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0] + 4 * out_xsize_plus_1[30] / 4;
#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[31][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[30][0] + output_WDMA_WRA_buffer_size[30][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[32][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[31][0] + output_WDMA_WRA_buffer_size[31][0]
	/ 4;
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FD old\n");
#else
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[32][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[30][0] + output_WDMA_WRA_buffer_size[30][0]
	/ 4;
#endif

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[33][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[32][0] + output_WDMA_WRA_buffer_size[32][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[33][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[33][0] + output_WDMA_WRA_buffer_size[33][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[34][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[33][2] + output_WDMA_WRA_buffer_size[33][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[34][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[34][0] + output_WDMA_WRA_buffer_size[34][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[35][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[34][2] + output_WDMA_WRA_buffer_size[34][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[35][0] + output_WDMA_WRA_buffer_size[35][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][0] + output_WDMA_WRA_buffer_size[36][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][1] + output_WDMA_WRA_buffer_size[36][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][2] + output_WDMA_WRA_buffer_size[36][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[36][3] + output_WDMA_WRA_buffer_size[36][3]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][0] + output_WDMA_WRA_buffer_size[37][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][1] + output_WDMA_WRA_buffer_size[37][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][2] + output_WDMA_WRA_buffer_size[37][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[38][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[37][3] + output_WDMA_WRA_buffer_size[37][3]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[39][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[38][0] + output_WDMA_WRA_buffer_size[38][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[39][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[39][0] + output_WDMA_WRA_buffer_size[39][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[40][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[39][2] + output_WDMA_WRA_buffer_size[39][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[40][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[40][0] + output_WDMA_WRA_buffer_size[40][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[41][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[40][1] + output_WDMA_WRA_buffer_size[40][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[42][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[41][0] + output_WDMA_WRA_buffer_size[41][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[42][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[42][0] + output_WDMA_WRA_buffer_size[42][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[43][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[42][2] + output_WDMA_WRA_buffer_size[42][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[43][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[43][0] + output_WDMA_WRA_buffer_size[43][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[44][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[43][1] + output_WDMA_WRA_buffer_size[43][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[45][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[44][0] + output_WDMA_WRA_buffer_size[44][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[46][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[45][0] + output_WDMA_WRA_buffer_size[45][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[47][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[46][0] + output_WDMA_WRA_buffer_size[46][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[48][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[47][0] + output_WDMA_WRA_buffer_size[47][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[49][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[48][0] + output_WDMA_WRA_buffer_size[48][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[50][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[49][0] + output_WDMA_WRA_buffer_size[49][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[50][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[50][0] + output_WDMA_WRA_buffer_size[50][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[50][1] + output_WDMA_WRA_buffer_size[50][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] + output_WDMA_WRA_buffer_size[51][0]
	/ 4 + output_WDMA_WRA_buffer_size[51][2] / 4 +
	output_WDMA_WRA_buffer_size[52][0] / 4 + output_WDMA_WRA_buffer_size[52][2] /
	4 + output_WDMA_WRA_buffer_size[53][0] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] + 1 * out_xsize_plus_1[51] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1] + 1 * out_xsize_plus_1[51] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[52][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] + 2 * out_xsize_plus_1[52] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[52][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1] + 2 * out_xsize_plus_1[52] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[52][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] + 3 * out_xsize_plus_1[52] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[52][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1] + 3 * out_xsize_plus_1[52] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[53][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0] + 4 * out_xsize_plus_1[53] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[53][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1] + 4 * out_xsize_plus_1[53] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[53][1] + output_WDMA_WRA_buffer_size[53][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0] + 1 * out_xsize_plus_1[54] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[55][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0] + 2 * out_xsize_plus_1[55] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[55][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0] + 3 * out_xsize_plus_1[55] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[56][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0] + 4 * out_xsize_plus_1[56] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[56][0] + output_WDMA_WRA_buffer_size[56][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][0] + 1 * out_xsize_plus_1[57] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[58][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][0] + 2 * out_xsize_plus_1[58] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[58][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][0] + 3 * out_xsize_plus_1[58] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[59][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[57][0] + 4 * out_xsize_plus_1[59] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[59][0] + output_WDMA_WRA_buffer_size[59][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0] + 1 * out_xsize_plus_1[60] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[61][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0] + 2 * out_xsize_plus_1[61] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[61][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0] + 3 * out_xsize_plus_1[61] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[62][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0] + 4 * out_xsize_plus_1[62] / 4;

#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[63][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[62][0] + output_WDMA_WRA_buffer_size[62][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[64][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[63][0] + output_WDMA_WRA_buffer_size[63][0]
	/ 4;
#else
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[64][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[62][0] + output_WDMA_WRA_buffer_size[62][0]
	/ 4;
#endif

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[65][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[64][0] + output_WDMA_WRA_buffer_size[64][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[65][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[65][0] + output_WDMA_WRA_buffer_size[65][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[66][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[65][2] + output_WDMA_WRA_buffer_size[65][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[66][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[66][0] + output_WDMA_WRA_buffer_size[66][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[67][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[66][2] + output_WDMA_WRA_buffer_size[66][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[67][0] + output_WDMA_WRA_buffer_size[67][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][0] + output_WDMA_WRA_buffer_size[68][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][1] + output_WDMA_WRA_buffer_size[68][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][2] + output_WDMA_WRA_buffer_size[68][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[68][3] + output_WDMA_WRA_buffer_size[68][3]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][0] + output_WDMA_WRA_buffer_size[69][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][1] + output_WDMA_WRA_buffer_size[69][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][2] + output_WDMA_WRA_buffer_size[69][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[70][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[69][3] + output_WDMA_WRA_buffer_size[69][3]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[71][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[70][0] + output_WDMA_WRA_buffer_size[70][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[71][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[71][0] + output_WDMA_WRA_buffer_size[71][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[72][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[71][2] + output_WDMA_WRA_buffer_size[71][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[72][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[72][0] + output_WDMA_WRA_buffer_size[72][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[73][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[72][1] + output_WDMA_WRA_buffer_size[72][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[74][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[73][0] + output_WDMA_WRA_buffer_size[73][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[74][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[74][0] + output_WDMA_WRA_buffer_size[74][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[75][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[74][2] + output_WDMA_WRA_buffer_size[74][2]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[75][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[75][0] + output_WDMA_WRA_buffer_size[75][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[76][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[75][1] + output_WDMA_WRA_buffer_size[75][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[77][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[76][0] + output_WDMA_WRA_buffer_size[76][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[78][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[77][0] + output_WDMA_WRA_buffer_size[77][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[79][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[78][0] + output_WDMA_WRA_buffer_size[78][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[80][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[79][0] + output_WDMA_WRA_buffer_size[79][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[81][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[80][0] + output_WDMA_WRA_buffer_size[80][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[82][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[81][0] + output_WDMA_WRA_buffer_size[81][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[82][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[82][0] + output_WDMA_WRA_buffer_size[82][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[82][1] + output_WDMA_WRA_buffer_size[82][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] + output_WDMA_WRA_buffer_size[83][0]
	/ 4 + output_WDMA_WRA_buffer_size[83][2] / 4 +
	output_WDMA_WRA_buffer_size[84][0] / 4 + output_WDMA_WRA_buffer_size[84][2] /
	4 + output_WDMA_WRA_buffer_size[85][0] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] + 1 * out_xsize_plus_1[83] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1] + 1 * out_xsize_plus_1[83] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[84][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] + 2 * out_xsize_plus_1[84] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[84][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1] + 2 * out_xsize_plus_1[84] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[84][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] + 3 * out_xsize_plus_1[84] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[84][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1] + 3 * out_xsize_plus_1[84] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[85][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0] + 4 * out_xsize_plus_1[85] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[85][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1] + 4 * out_xsize_plus_1[85] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[85][1] + output_WDMA_WRA_buffer_size[85][1]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0] + 1 * out_xsize_plus_1[86] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[87][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0] + 2 * out_xsize_plus_1[87] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[87][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0] + 3 * out_xsize_plus_1[87] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[88][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0] + 4 * out_xsize_plus_1[88] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[88][0] + output_WDMA_WRA_buffer_size[88][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][0] + 1 * out_xsize_plus_1[89] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[90][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][0] + 2 * out_xsize_plus_1[90] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[90][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][0] + 3 * out_xsize_plus_1[90] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[91][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[89][0] + 4 * out_xsize_plus_1[91] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[91][0] + output_WDMA_WRA_buffer_size[91][0]
	/ 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0] + 1 * out_xsize_plus_1[92] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[93][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0] + 2 * out_xsize_plus_1[93] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[93][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0] + 3 * out_xsize_plus_1[93] / 4;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[94][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0] + 4 * out_xsize_plus_1[94] / 4;

#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[95][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[94][0] + output_WDMA_WRA_buffer_size[94][0]
	/ 4;
	nextPA = g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[95][0] +
	output_WDMA_WRA_buffer_size[95][0] / 4;
#else
	nextPA = g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[94][0] +
	output_WDMA_WRA_buffer_size[94][0] / 4;
#endif

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[fdvt_fd_rpn2_loop_num][0] = currentResultPA;
	//g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[fdvt_fd_rpn2_loop_num][0] = currentResultVA;
	currentResultPA = currentResultPA +
	output_WDMA_WRA_buffer_size[fdvt_fd_rpn2_loop_num][0]/4;
	//currentResultVA = currentResultVA + output_WDMA_WRA_buffer_size[fdvt_fd_rpn2_loop_num][0]/8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[fdvt_fd_rpn1_loop_num][0] = currentResultPA;
	//g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[fdvt_fd_rpn1_loop_num][0] = currentResultVA;
	currentResultPA = currentResultPA +
	output_WDMA_WRA_buffer_size[fdvt_fd_rpn1_loop_num][0]/4;
	//currentResultVA = currentResultVA + output_WDMA_WRA_buffer_size[fdvt_fd_rpn1_loop_num][0]/8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[fdvt_fd_rpn0_loop_num][0] = currentResultPA;
	//g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[fdvt_fd_rpn0_loop_num][0] = currentResultVA;
	currentResultPA = currentResultPA +
	output_WDMA_WRA_buffer_size[fdvt_fd_rpn0_loop_num][0]/4;
	//currentResultVA = currentResultVA + output_WDMA_WRA_buffer_size[fdvt_fd_rpn0_loop_num][0]/8;

	// secure FD pose result
	for (i = 0; i < 3; i++) {
		g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_PA[i][0] = currentResultPA;
		//g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_VA[i][0] = currentResultVA;
		currentResultPA = g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_PA[i][0] +
		fdvt_fd_result_size / 4;
		//currentResultVA = g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_VA[i][0] + fdvt_fd_result_size / 8;
	}

	for(i=0; i < fdvt_fd_loop_num; i++) {
		for(j=0; j < kernel_RDMA_RA_num ; j++) {
			if(kernel_RDMA_RA_buffer_en[i][j] == 1) {
				currentPA = nextPA;
				g_FdDrv_Fd_DMA_Para->fd_kernel_PA[i][j] = currentPA;
				nextPA = g_FdDrv_Fd_DMA_Para->fd_kernel_PA[i][j] +
				kernel_RDMA_RA_buffer_size[i][j]/4;
			}
		}
	}
}

void FDVT_arrangeOutputAddress_PKVM_PA(struct FDVT_SEC_MetaDataToGCE
*FDVT_MetaData)
{


	uint64_t *currentPA = NULL, *nextPA = NULL;
	uint8_t i=0, j=0;

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[0][0] = (uint64_t *)(uintptr_t
	)FDVT_MetaData->FDOutBuf_Handler;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[1][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[0][0] +
	output_WDMA_WRA_buffer_size[0][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[1][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[1][0] +
	output_WDMA_WRA_buffer_size[1][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[2][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[1][2] +
	output_WDMA_WRA_buffer_size[1][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[2][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[2][0] +
	output_WDMA_WRA_buffer_size[2][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[3][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[2][2] +
	output_WDMA_WRA_buffer_size[2][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[3][0] +
	output_WDMA_WRA_buffer_size[3][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][0] +
	output_WDMA_WRA_buffer_size[4][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][1] +
	output_WDMA_WRA_buffer_size[4][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][2] +
	output_WDMA_WRA_buffer_size[4][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[4][3] +
	output_WDMA_WRA_buffer_size[4][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][0] +
	output_WDMA_WRA_buffer_size[5][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][1] +
	output_WDMA_WRA_buffer_size[5][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][2] +
	output_WDMA_WRA_buffer_size[5][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[6][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[5][3] +
	output_WDMA_WRA_buffer_size[5][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[7][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[6][0] +
	output_WDMA_WRA_buffer_size[6][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[7][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[7][0] +
	output_WDMA_WRA_buffer_size[7][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[8][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[7][2] +
	output_WDMA_WRA_buffer_size[7][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[8][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[8][0] +
	output_WDMA_WRA_buffer_size[8][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[9][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[8][1] +
	output_WDMA_WRA_buffer_size[8][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[10][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[9][0] +
	output_WDMA_WRA_buffer_size[9][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[10][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[10][0] +
	output_WDMA_WRA_buffer_size[10][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[11][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[10][2] +
	output_WDMA_WRA_buffer_size[10][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[11][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[11][0] +
	output_WDMA_WRA_buffer_size[11][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[12][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[11][1] +
	output_WDMA_WRA_buffer_size[11][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[13][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[12][0] +
	output_WDMA_WRA_buffer_size[12][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[14][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[13][0] +
	output_WDMA_WRA_buffer_size[13][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[15][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[14][0] +
	output_WDMA_WRA_buffer_size[14][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[16][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[15][0] +
	output_WDMA_WRA_buffer_size[15][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[17][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[16][0] +
	output_WDMA_WRA_buffer_size[16][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[18][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[17][0] +
	output_WDMA_WRA_buffer_size[17][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[18][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[18][0] +
	output_WDMA_WRA_buffer_size[18][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[18][1] +
	output_WDMA_WRA_buffer_size[18][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] +
	output_WDMA_WRA_buffer_size[19][0] / 8 + output_WDMA_WRA_buffer_size[19][2] /
	8 + output_WDMA_WRA_buffer_size[20][0] / 8 +
	output_WDMA_WRA_buffer_size[20][2] / 8 + output_WDMA_WRA_buffer_size[21][0] /
	8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] + 1 * out_xsize_plus_1[19] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][1] + 1 * out_xsize_plus_1[19] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[20][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] + 2 * out_xsize_plus_1[20] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[20][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][1] + 2 * out_xsize_plus_1[20] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[20][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] + 3 * out_xsize_plus_1[20] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[20][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][1] + 3 * out_xsize_plus_1[20] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[21][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][0] + 4 * out_xsize_plus_1[21] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[21][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[19][1] + 4 * out_xsize_plus_1[21] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[21][1] +
	output_WDMA_WRA_buffer_size[21][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][0] + 1 * out_xsize_plus_1[22] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[23][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][0] + 2 * out_xsize_plus_1[23] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[23][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][0] + 3 * out_xsize_plus_1[23] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[24][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[22][0] + 4 * out_xsize_plus_1[24] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[24][0] +
	output_WDMA_WRA_buffer_size[24][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][0] + 1 * out_xsize_plus_1[25] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[26][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][0] + 2 * out_xsize_plus_1[26] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[26][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][0] + 3 * out_xsize_plus_1[26] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[27][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[25][0] + 4 * out_xsize_plus_1[27] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[27][0] +
	output_WDMA_WRA_buffer_size[27][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][0] + 1 * out_xsize_plus_1[28] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[29][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][0] + 2 * out_xsize_plus_1[29] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[29][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][0] + 3 * out_xsize_plus_1[29] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[30][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[28][0] + 4 * out_xsize_plus_1[30] / 8;

#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[31][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[30][0] +
	output_WDMA_WRA_buffer_size[30][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[32][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[31][0] +
	output_WDMA_WRA_buffer_size[31][0] / 8;
#else
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[32][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[30][0] +
	output_WDMA_WRA_buffer_size[30][0] / 8;
#endif

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[33][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[32][0] +
	output_WDMA_WRA_buffer_size[32][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[33][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[33][0] +
	output_WDMA_WRA_buffer_size[33][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[34][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[33][2] +
	output_WDMA_WRA_buffer_size[33][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[34][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[34][0] +
	output_WDMA_WRA_buffer_size[34][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[35][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[34][2] +
	output_WDMA_WRA_buffer_size[34][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[35][0] +
	output_WDMA_WRA_buffer_size[35][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][0] +
	output_WDMA_WRA_buffer_size[36][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][1] +
	output_WDMA_WRA_buffer_size[36][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][2] +
	output_WDMA_WRA_buffer_size[36][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[36][3] +
	output_WDMA_WRA_buffer_size[36][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][0] +
	output_WDMA_WRA_buffer_size[37][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][1] +
	output_WDMA_WRA_buffer_size[37][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][2] +
	output_WDMA_WRA_buffer_size[37][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[38][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[37][3] +
	output_WDMA_WRA_buffer_size[37][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[39][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[38][0] +
	output_WDMA_WRA_buffer_size[38][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[39][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[39][0] +
	output_WDMA_WRA_buffer_size[39][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[40][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[39][2] +
	output_WDMA_WRA_buffer_size[39][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[40][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[40][0] +
	output_WDMA_WRA_buffer_size[40][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[41][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[40][1] +
	output_WDMA_WRA_buffer_size[40][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[42][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[41][0] +
	output_WDMA_WRA_buffer_size[41][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[42][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[42][0] +
	output_WDMA_WRA_buffer_size[42][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[43][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[42][2] +
	output_WDMA_WRA_buffer_size[42][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[43][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[43][0] +
	output_WDMA_WRA_buffer_size[43][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[44][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[43][1] +
	output_WDMA_WRA_buffer_size[43][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[45][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[44][0] +
	output_WDMA_WRA_buffer_size[44][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[46][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[45][0] +
	output_WDMA_WRA_buffer_size[45][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[47][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[46][0] +
	output_WDMA_WRA_buffer_size[46][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[48][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[47][0] +
	output_WDMA_WRA_buffer_size[47][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[49][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[48][0] +
	output_WDMA_WRA_buffer_size[48][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[50][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[49][0] +
	output_WDMA_WRA_buffer_size[49][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[50][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[50][0] +
	output_WDMA_WRA_buffer_size[50][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[50][1] +
	output_WDMA_WRA_buffer_size[50][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] +
	output_WDMA_WRA_buffer_size[51][0] / 8 + output_WDMA_WRA_buffer_size[51][2] /
	8 + output_WDMA_WRA_buffer_size[52][0] / 8 +
	output_WDMA_WRA_buffer_size[52][2] / 8 + output_WDMA_WRA_buffer_size[53][0] /
	8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] + 1 * out_xsize_plus_1[51] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][1] + 1 * out_xsize_plus_1[51] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[52][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] + 2 * out_xsize_plus_1[52] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[52][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][1] + 2 * out_xsize_plus_1[52] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[52][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] + 3 * out_xsize_plus_1[52] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[52][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][1] + 3 * out_xsize_plus_1[52] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[53][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][0] + 4 * out_xsize_plus_1[53] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[53][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[51][1] + 4 * out_xsize_plus_1[53] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[53][1] +
	output_WDMA_WRA_buffer_size[53][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][0] + 1 * out_xsize_plus_1[54] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[55][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][0] + 2 * out_xsize_plus_1[55] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[55][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][0] + 3 * out_xsize_plus_1[55] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[56][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[54][0] + 4 * out_xsize_plus_1[56] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[56][0] +
	output_WDMA_WRA_buffer_size[56][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][0] + 1 * out_xsize_plus_1[57] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[58][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][0] + 2 * out_xsize_plus_1[58] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[58][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][0] + 3 * out_xsize_plus_1[58] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[59][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[57][0] + 4 * out_xsize_plus_1[59] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[59][0] +
	output_WDMA_WRA_buffer_size[59][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][0] + 1 * out_xsize_plus_1[60] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[61][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][0] + 2 * out_xsize_plus_1[61] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[61][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][0] + 3 * out_xsize_plus_1[61] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[62][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[60][0] + 4 * out_xsize_plus_1[62] / 8;

#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[63][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[62][0] +
	output_WDMA_WRA_buffer_size[62][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[64][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[63][0] +
	output_WDMA_WRA_buffer_size[63][0] / 8;
#else
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[64][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[62][0] +
	output_WDMA_WRA_buffer_size[62][0] / 8;
#endif

	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[65][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[64][0] +
	output_WDMA_WRA_buffer_size[64][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[65][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[65][0] +
	output_WDMA_WRA_buffer_size[65][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[66][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[65][2] +
	output_WDMA_WRA_buffer_size[65][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[66][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[66][0] +
	output_WDMA_WRA_buffer_size[66][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[67][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[66][2] +
	output_WDMA_WRA_buffer_size[66][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[67][0] +
	output_WDMA_WRA_buffer_size[67][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][0] +
	output_WDMA_WRA_buffer_size[68][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][1] +
	output_WDMA_WRA_buffer_size[68][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][2] +
	output_WDMA_WRA_buffer_size[68][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[68][3] +
	output_WDMA_WRA_buffer_size[68][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][0] +
	output_WDMA_WRA_buffer_size[69][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][1] +
	output_WDMA_WRA_buffer_size[69][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][2] +
	output_WDMA_WRA_buffer_size[69][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[70][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[69][3] +
	output_WDMA_WRA_buffer_size[69][3] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[71][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[70][0] +
	output_WDMA_WRA_buffer_size[70][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[71][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[71][0] +
	output_WDMA_WRA_buffer_size[71][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[72][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[71][2] +
	output_WDMA_WRA_buffer_size[71][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[72][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[72][0] +
	output_WDMA_WRA_buffer_size[72][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[73][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[72][1] +
	output_WDMA_WRA_buffer_size[72][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[74][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[73][0] +
	output_WDMA_WRA_buffer_size[73][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[74][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[74][0] +
	output_WDMA_WRA_buffer_size[74][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[75][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[74][2] +
	output_WDMA_WRA_buffer_size[74][2] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[75][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[75][0] +
	output_WDMA_WRA_buffer_size[75][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[76][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[75][1] +
	output_WDMA_WRA_buffer_size[75][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[77][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[76][0] +
	output_WDMA_WRA_buffer_size[76][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[78][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[77][0] +
	output_WDMA_WRA_buffer_size[77][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[79][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[78][0] +
	output_WDMA_WRA_buffer_size[78][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[80][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[79][0] +
	output_WDMA_WRA_buffer_size[79][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[81][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[80][0] +
	output_WDMA_WRA_buffer_size[80][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[82][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[81][0] +
	output_WDMA_WRA_buffer_size[81][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[82][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[82][0] +
	output_WDMA_WRA_buffer_size[82][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[82][1] +
	output_WDMA_WRA_buffer_size[82][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] +
	output_WDMA_WRA_buffer_size[83][0] / 8 + output_WDMA_WRA_buffer_size[83][2] /
	8 + output_WDMA_WRA_buffer_size[84][0] / 8 +
	output_WDMA_WRA_buffer_size[84][2] / 8 + output_WDMA_WRA_buffer_size[85][0] /
	8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] + 1 * out_xsize_plus_1[83] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][1] + 1 * out_xsize_plus_1[83] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[84][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] + 2 * out_xsize_plus_1[84] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[84][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][1] + 2 * out_xsize_plus_1[84] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[84][2] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] + 3 * out_xsize_plus_1[84] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[84][3] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][1] + 3 * out_xsize_plus_1[84] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[85][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][0] + 4 * out_xsize_plus_1[85] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[85][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[83][1] + 4 * out_xsize_plus_1[85] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[85][1] +
	output_WDMA_WRA_buffer_size[85][1] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][0] + 1 * out_xsize_plus_1[86] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[87][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][0] + 2 * out_xsize_plus_1[87] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[87][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][0] + 3 * out_xsize_plus_1[87] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[88][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[86][0] + 4 * out_xsize_plus_1[88] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[88][0] +
	output_WDMA_WRA_buffer_size[88][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][0] + 1 * out_xsize_plus_1[89] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[90][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][0] + 2 * out_xsize_plus_1[90] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[90][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][0] + 3 * out_xsize_plus_1[90] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[91][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[89][0] + 4 * out_xsize_plus_1[91] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[91][0] +
	output_WDMA_WRA_buffer_size[91][0] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][0] + 1 * out_xsize_plus_1[92] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[93][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][0] + 2 * out_xsize_plus_1[93] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[93][1] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][0] + 3 * out_xsize_plus_1[93] / 8;
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[94][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[92][0] + 4 * out_xsize_plus_1[94] / 8;

#ifndef NEW_BUFFER
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[95][0] =
	g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[94][0] +
	output_WDMA_WRA_buffer_size[94][0] / 8;
	nextPA = g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[95][0] +
	output_WDMA_WRA_buffer_size[95][0] / 8;
#else
	nextPA = g_FdDrv_Fd_DMA_Para->fd_out_hw_PKVM_PA[94][0] +
	output_WDMA_WRA_buffer_size[94][0] / 8;
#endif

	for(i=0; i < fdvt_fd_loop_num; i++) {
		for(j=0; j < kernel_RDMA_RA_num ; j++) {
			if(kernel_RDMA_RA_buffer_en[i][j] == 1) {
				currentPA = nextPA;
				g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[i][j] = currentPA;
				nextPA = g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[i][j] +
				kernel_RDMA_RA_buffer_size[i][j]/8;
			}
		}
	}
}

void fdvt_memcpy_ops(uint8_t *pa, uint8_t *src, int32_t size)
{
	uint32_t remain = 0;
	int32_t copy_size = size;
	uint8_t *va = NULL;
	uint8_t *tmp_pa = pa;
	//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "memcpy from start PA = ");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)tmp_pa);
	//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "memcpy size = ");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)size);
	while (copy_size > 0) {
		//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "memcpy from PA = ");
		//CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)tmp_pa);
		//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "memcpy copy_size = ");
		//CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)copy_size);
		//MAP
		va = CALL_FROM_CMDQ_FDVT_OPS(fixmap_map, (u64)tmp_pa);
		remain = 0x1000 - ((uint32_t)(uintptr_t)va & 0xFFF);
		//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "offset = ");
		if (copy_size < remain)
			remain = copy_size;

		//CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)((u64)(uintptr_t)tmp_pa-(u64)(uintptr_t)pa));
		CALL_FROM_CMDQ_FDVT_OPS(memcpy, va, src + ((u64)(uintptr_t)tmp_pa-(u64
		)(uintptr_t)pa), remain);
		//UNMAP
		//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "flush VA, remain");
		//CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)remain);
		CALL_FROM_CMDQ_FDVT_OPS(flush_dcache_to_poc, (void *)(uintptr_t)va, remain);
		//CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "UNMAP VA");
		//CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)va);
		CALL_FROM_CMDQ_FDVT_OPS(fixmap_unmap);
		tmp_pa += remain;
		copy_size -= remain;
	}

}

void FDVT_copyInputDataToBuffer(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData)
{
	//unsigned char local_kernel_array[sizeof(fd_kernel_bias_loop00_0_frame01)];

	if (FDVT_MetaData->FDMode == 0) {
		// Copy kernel channel data
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[0][0],
		(uint8_t *)(fd_kernel_bias_loop00_0_frame01),
		sizeof(fd_kernel_bias_loop00_0_frame01));

		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[0][1],
		(uint8_t *)(fd_kernel_bias_loop00_1_frame01),
		sizeof(fd_kernel_bias_loop00_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[1][0],
		(uint8_t *)(fd_kernel_bias_loop01_0_frame01),
		sizeof(fd_kernel_bias_loop01_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[1][1],
		(uint8_t *)(fd_kernel_bias_loop01_1_frame01),
		sizeof(fd_kernel_bias_loop01_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[2][0],
		(uint8_t *)(fd_kernel_bias_loop02_0_frame01),
		sizeof(fd_kernel_bias_loop02_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[2][1],
		(uint8_t *)(fd_kernel_bias_loop02_1_frame01),
		sizeof(fd_kernel_bias_loop02_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[3][0],
		(uint8_t *)(fd_kernel_bias_loop03_0_frame01),
		sizeof(fd_kernel_bias_loop03_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[3][1],
		(uint8_t *)(fd_kernel_bias_loop03_1_frame01),
		sizeof(fd_kernel_bias_loop03_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[4][0],
		(uint8_t *)(fd_kernel_bias_loop04_0_frame01),
		sizeof(fd_kernel_bias_loop04_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[4][1],
		(uint8_t *)(fd_kernel_bias_loop04_1_frame01),
		sizeof(fd_kernel_bias_loop04_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[5][0],
		(uint8_t *)(fd_kernel_bias_loop05_0_frame01),
		sizeof(fd_kernel_bias_loop05_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[5][1],
		(uint8_t *)(fd_kernel_bias_loop05_1_frame01),
		sizeof(fd_kernel_bias_loop05_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[6][0],
		(uint8_t *)(fd_kernel_bias_loop06_0_frame01),
		sizeof(fd_kernel_bias_loop06_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[6][1],
		(uint8_t *)(fd_kernel_bias_loop06_1_frame01),
		sizeof(fd_kernel_bias_loop06_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[7][0],
		(uint8_t *)(fd_kernel_bias_loop07_0_frame01),
		sizeof(fd_kernel_bias_loop07_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[7][1],
		(uint8_t *)(fd_kernel_bias_loop07_1_frame01),
		sizeof(fd_kernel_bias_loop07_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[8][0],
		(uint8_t *)(fd_kernel_bias_loop08_0_frame01),
		sizeof(fd_kernel_bias_loop08_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[8][1],
		(uint8_t *)(fd_kernel_bias_loop08_1_frame01),
		sizeof(fd_kernel_bias_loop08_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[9][0],
		(uint8_t *)(fd_kernel_bias_loop09_0_frame01),
		sizeof(fd_kernel_bias_loop09_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[9][1],
		(uint8_t *)(fd_kernel_bias_loop09_1_frame01),
		sizeof(fd_kernel_bias_loop09_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[10][0],
		(uint8_t *)(fd_kernel_bias_loop10_0_frame01),
		sizeof(fd_kernel_bias_loop10_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[10][1],
		(uint8_t *)(fd_kernel_bias_loop10_1_frame01),
		sizeof(fd_kernel_bias_loop10_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[11][0],
		(uint8_t *)(fd_kernel_bias_loop11_0_frame01),
		sizeof(fd_kernel_bias_loop11_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[11][1],
		(uint8_t *)(fd_kernel_bias_loop11_1_frame01),
		sizeof(fd_kernel_bias_loop11_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[12][0],
		(uint8_t *)(fd_kernel_bias_loop12_0_frame01),
		sizeof(fd_kernel_bias_loop12_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[12][1],
		(uint8_t *)(fd_kernel_bias_loop12_1_frame01),
		sizeof(fd_kernel_bias_loop12_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[13][0],
		(uint8_t *)(fd_kernel_bias_loop13_0_frame01),
		sizeof(fd_kernel_bias_loop13_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[13][1],
		(uint8_t *)(fd_kernel_bias_loop13_1_frame01),
		sizeof(fd_kernel_bias_loop13_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[14][0],
		(uint8_t *)(fd_kernel_bias_loop14_0_frame01),
		sizeof(fd_kernel_bias_loop14_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[14][1],
		(uint8_t *)(fd_kernel_bias_loop14_1_frame01),
		sizeof(fd_kernel_bias_loop14_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[15][0],
		(uint8_t *)(fd_kernel_bias_loop15_0_frame01),
		sizeof(fd_kernel_bias_loop15_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[15][1],
		(uint8_t *)(fd_kernel_bias_loop15_1_frame01),
		sizeof(fd_kernel_bias_loop15_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[16][0],
		(uint8_t *)(fd_kernel_bias_loop16_0_frame01),
		sizeof(fd_kernel_bias_loop16_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[16][1],
		(uint8_t *)(fd_kernel_bias_loop16_1_frame01),
		sizeof(fd_kernel_bias_loop16_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[17][0],
		(uint8_t *)(fd_kernel_bias_loop17_0_frame01),
		sizeof(fd_kernel_bias_loop17_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[17][1],
		(uint8_t *)(fd_kernel_bias_loop17_1_frame01),
		sizeof(fd_kernel_bias_loop17_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[18][0],
		(uint8_t *)(fd_kernel_bias_loop18_0_frame01),
		sizeof(fd_kernel_bias_loop18_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[18][1],
		(uint8_t *)(fd_kernel_bias_loop18_1_frame01),
		sizeof(fd_kernel_bias_loop18_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[19][0],
		(uint8_t *)(fd_kernel_bias_loop19_0_frame01),
		sizeof(fd_kernel_bias_loop19_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[19][1],
		(uint8_t *)(fd_kernel_bias_loop19_1_frame01),
		sizeof(fd_kernel_bias_loop19_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[20][0],
		(uint8_t *)(fd_kernel_bias_loop20_0_frame01),
		sizeof(fd_kernel_bias_loop20_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[20][1],
		(uint8_t *)(fd_kernel_bias_loop20_1_frame01),
		sizeof(fd_kernel_bias_loop20_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[21][0],
		(uint8_t *)(fd_kernel_bias_loop21_0_frame01),
		sizeof(fd_kernel_bias_loop21_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[21][1],
		(uint8_t *)(fd_kernel_bias_loop21_1_frame01),
		sizeof(fd_kernel_bias_loop21_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[22][0],
		(uint8_t *)(fd_kernel_bias_loop22_0_frame01),
		sizeof(fd_kernel_bias_loop22_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[22][1],
		(uint8_t *)(fd_kernel_bias_loop22_1_frame01),
		sizeof(fd_kernel_bias_loop22_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[23][0],
		(uint8_t *)(fd_kernel_bias_loop23_0_frame01),
		sizeof(fd_kernel_bias_loop23_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[23][1],
		(uint8_t *)(fd_kernel_bias_loop23_1_frame01),
		sizeof(fd_kernel_bias_loop23_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[24][0],
		(uint8_t *)(fd_kernel_bias_loop24_0_frame01),
		sizeof(fd_kernel_bias_loop24_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[24][1],
		(uint8_t *)(fd_kernel_bias_loop24_1_frame01),
		sizeof(fd_kernel_bias_loop24_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[25][0],
		(uint8_t *)(fd_kernel_bias_loop25_0_frame01),
		sizeof(fd_kernel_bias_loop25_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[25][1],
		(uint8_t *)(fd_kernel_bias_loop25_1_frame01),
		sizeof(fd_kernel_bias_loop25_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[26][0],
		(uint8_t *)(fd_kernel_bias_loop26_0_frame01),
		sizeof(fd_kernel_bias_loop26_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[26][1],
		(uint8_t *)(fd_kernel_bias_loop26_1_frame01),
		sizeof(fd_kernel_bias_loop26_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[27][0],
		(uint8_t *)(fd_kernel_bias_loop27_0_frame01),
		sizeof(fd_kernel_bias_loop27_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[27][1],
		(uint8_t *)(fd_kernel_bias_loop27_1_frame01),
		sizeof(fd_kernel_bias_loop27_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[28][0],
		(uint8_t *)(fd_kernel_bias_loop28_0_frame01),
		sizeof(fd_kernel_bias_loop28_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[28][1],
		(uint8_t *)(fd_kernel_bias_loop28_1_frame01),
		sizeof(fd_kernel_bias_loop28_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[29][0],
		(uint8_t *)(fd_kernel_bias_loop29_0_frame01),
		sizeof(fd_kernel_bias_loop29_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[29][1],
		(uint8_t *)(fd_kernel_bias_loop29_1_frame01),
		sizeof(fd_kernel_bias_loop29_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[30][0],
		(uint8_t *)(fd_kernel_bias_loop30_0_frame01),
		sizeof(fd_kernel_bias_loop30_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[30][1],
		(uint8_t *)(fd_kernel_bias_loop30_1_frame01),
		sizeof(fd_kernel_bias_loop30_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[32][0],
		(uint8_t *)(fd_kernel_bias_loop32_0_frame01),
		sizeof(fd_kernel_bias_loop32_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[32][1],
		(uint8_t *)(fd_kernel_bias_loop32_1_frame01),
		sizeof(fd_kernel_bias_loop32_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[33][0],
		(uint8_t *)(fd_kernel_bias_loop33_0_frame01),
		sizeof(fd_kernel_bias_loop33_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[33][1],
		(uint8_t *)(fd_kernel_bias_loop33_1_frame01),
		sizeof(fd_kernel_bias_loop33_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[34][0],
		(uint8_t *)(fd_kernel_bias_loop34_0_frame01),
		sizeof(fd_kernel_bias_loop34_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[34][1],
		(uint8_t *)(fd_kernel_bias_loop34_1_frame01),
		sizeof(fd_kernel_bias_loop34_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[35][0],
		(uint8_t *)(fd_kernel_bias_loop35_0_frame01),
		sizeof(fd_kernel_bias_loop35_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[35][1],
		(uint8_t *)(fd_kernel_bias_loop35_1_frame01),
		sizeof(fd_kernel_bias_loop35_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[36][0],
		(uint8_t *)(fd_kernel_bias_loop36_0_frame01),
		sizeof(fd_kernel_bias_loop36_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[36][1],
		(uint8_t *)(fd_kernel_bias_loop36_1_frame01),
		sizeof(fd_kernel_bias_loop36_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[37][0],
		(uint8_t *)(fd_kernel_bias_loop37_0_frame01),
		sizeof(fd_kernel_bias_loop37_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[37][1],
		(uint8_t *)(fd_kernel_bias_loop37_1_frame01),
		sizeof(fd_kernel_bias_loop37_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[38][0],
		(uint8_t *)(fd_kernel_bias_loop38_0_frame01),
		sizeof(fd_kernel_bias_loop38_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[38][1],
		(uint8_t *)(fd_kernel_bias_loop38_1_frame01),
		sizeof(fd_kernel_bias_loop38_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[39][0],
		(uint8_t *)(fd_kernel_bias_loop39_0_frame01),
		sizeof(fd_kernel_bias_loop39_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[39][1],
		(uint8_t *)(fd_kernel_bias_loop39_1_frame01),
		sizeof(fd_kernel_bias_loop39_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[40][0],
		(uint8_t *)(fd_kernel_bias_loop40_0_frame01),
		sizeof(fd_kernel_bias_loop40_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[40][1],
		(uint8_t *)(fd_kernel_bias_loop40_1_frame01),
		sizeof(fd_kernel_bias_loop40_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[41][0],
		(uint8_t *)(fd_kernel_bias_loop41_0_frame01),
		sizeof(fd_kernel_bias_loop41_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[41][1],
		(uint8_t *)(fd_kernel_bias_loop41_1_frame01),
		sizeof(fd_kernel_bias_loop41_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[42][0],
		(uint8_t *)(fd_kernel_bias_loop42_0_frame01),
		sizeof(fd_kernel_bias_loop42_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[42][1],
		(uint8_t *)(fd_kernel_bias_loop42_1_frame01),
		sizeof(fd_kernel_bias_loop42_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[43][0],
		(uint8_t *)(fd_kernel_bias_loop43_0_frame01),
		sizeof(fd_kernel_bias_loop43_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[43][1],
		(uint8_t *)(fd_kernel_bias_loop43_1_frame01),
		sizeof(fd_kernel_bias_loop43_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[44][0],
		(uint8_t *)(fd_kernel_bias_loop44_0_frame01),
		sizeof(fd_kernel_bias_loop44_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[44][1],
		(uint8_t *)(fd_kernel_bias_loop44_1_frame01),
		sizeof(fd_kernel_bias_loop44_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[45][0],
		(uint8_t *)(fd_kernel_bias_loop45_0_frame01),
		sizeof(fd_kernel_bias_loop45_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[45][1],
		(uint8_t *)(fd_kernel_bias_loop45_1_frame01),
		sizeof(fd_kernel_bias_loop45_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[46][0],
		(uint8_t *)(fd_kernel_bias_loop46_0_frame01),
		sizeof(fd_kernel_bias_loop46_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[46][1],
		(uint8_t *)(fd_kernel_bias_loop46_1_frame01),
		sizeof(fd_kernel_bias_loop46_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[47][0],
		(uint8_t *)(fd_kernel_bias_loop47_0_frame01),
		sizeof(fd_kernel_bias_loop47_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[47][1],
		(uint8_t *)(fd_kernel_bias_loop47_1_frame01),
		sizeof(fd_kernel_bias_loop47_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[48][0],
		(uint8_t *)(fd_kernel_bias_loop48_0_frame01),
		sizeof(fd_kernel_bias_loop48_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[48][1],
		(uint8_t *)(fd_kernel_bias_loop48_1_frame01),
		sizeof(fd_kernel_bias_loop48_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[49][0],
		(uint8_t *)(fd_kernel_bias_loop49_0_frame01),
		sizeof(fd_kernel_bias_loop49_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[49][1],
		(uint8_t *)(fd_kernel_bias_loop49_1_frame01),
		sizeof(fd_kernel_bias_loop49_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[50][0],
		(uint8_t *)(fd_kernel_bias_loop50_0_frame01),
		sizeof(fd_kernel_bias_loop50_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[50][1],
		(uint8_t *)(fd_kernel_bias_loop50_1_frame01),
		sizeof(fd_kernel_bias_loop50_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[51][0],
		(uint8_t *)(fd_kernel_bias_loop51_0_frame01),
		sizeof(fd_kernel_bias_loop51_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[51][1],
		(uint8_t *)(fd_kernel_bias_loop51_1_frame01),
		sizeof(fd_kernel_bias_loop51_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[52][0],
		(uint8_t *)(fd_kernel_bias_loop52_0_frame01),
		sizeof(fd_kernel_bias_loop52_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[52][1],
		(uint8_t *)(fd_kernel_bias_loop52_1_frame01),
		sizeof(fd_kernel_bias_loop52_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[53][0],
		(uint8_t *)(fd_kernel_bias_loop53_0_frame01),
		sizeof(fd_kernel_bias_loop53_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[53][1],
		(uint8_t *)(fd_kernel_bias_loop53_1_frame01),
		sizeof(fd_kernel_bias_loop53_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[54][0],
		(uint8_t *)(fd_kernel_bias_loop54_0_frame01),
		sizeof(fd_kernel_bias_loop54_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[54][1],
		(uint8_t *)(fd_kernel_bias_loop54_1_frame01),
		sizeof(fd_kernel_bias_loop54_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[55][0],
		(uint8_t *)(fd_kernel_bias_loop55_0_frame01),
		sizeof(fd_kernel_bias_loop55_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[55][1],
		(uint8_t *)(fd_kernel_bias_loop55_1_frame01),
		sizeof(fd_kernel_bias_loop55_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[56][0],
		(uint8_t *)(fd_kernel_bias_loop56_0_frame01),
		sizeof(fd_kernel_bias_loop56_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[56][1],
		(uint8_t *)(fd_kernel_bias_loop56_1_frame01),
		sizeof(fd_kernel_bias_loop56_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[57][0],
		(uint8_t *)(fd_kernel_bias_loop57_0_frame01),
		sizeof(fd_kernel_bias_loop57_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[57][1],
		(uint8_t *)(fd_kernel_bias_loop57_1_frame01),
		sizeof(fd_kernel_bias_loop57_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[58][0],
		(uint8_t *)(fd_kernel_bias_loop58_0_frame01),
		sizeof(fd_kernel_bias_loop58_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[58][1],
		(uint8_t *)(fd_kernel_bias_loop58_1_frame01),
		sizeof(fd_kernel_bias_loop58_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[59][0],
		(uint8_t *)(fd_kernel_bias_loop59_0_frame01),
		sizeof(fd_kernel_bias_loop59_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[59][1],
		(uint8_t *)(fd_kernel_bias_loop59_1_frame01),
		sizeof(fd_kernel_bias_loop59_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[60][0],
		(uint8_t *)(fd_kernel_bias_loop60_0_frame01),
		sizeof(fd_kernel_bias_loop60_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[60][1],
		(uint8_t *)(fd_kernel_bias_loop60_1_frame01),
		sizeof(fd_kernel_bias_loop60_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[61][0],
		(uint8_t *)(fd_kernel_bias_loop61_0_frame01),
		sizeof(fd_kernel_bias_loop61_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[61][1],
		(uint8_t *)(fd_kernel_bias_loop61_1_frame01),
		sizeof(fd_kernel_bias_loop61_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[62][0],
		(uint8_t *)(fd_kernel_bias_loop62_0_frame01),
		sizeof(fd_kernel_bias_loop62_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[62][1],
		(uint8_t *)(fd_kernel_bias_loop62_1_frame01),
		sizeof(fd_kernel_bias_loop62_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[64][0],
		(uint8_t *)(fd_kernel_bias_loop64_0_frame01),
		sizeof(fd_kernel_bias_loop64_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[64][1],
		(uint8_t *)(fd_kernel_bias_loop64_1_frame01),
		sizeof(fd_kernel_bias_loop64_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[65][0],
		(uint8_t *)(fd_kernel_bias_loop65_0_frame01),
		sizeof(fd_kernel_bias_loop65_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[65][1],
		(uint8_t *)(fd_kernel_bias_loop65_1_frame01),
		sizeof(fd_kernel_bias_loop65_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[66][0],
		(uint8_t *)(fd_kernel_bias_loop66_0_frame01),
		sizeof(fd_kernel_bias_loop66_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[66][1],
		(uint8_t *)(fd_kernel_bias_loop66_1_frame01),
		sizeof(fd_kernel_bias_loop66_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[67][0],
		(uint8_t *)(fd_kernel_bias_loop67_0_frame01),
		sizeof(fd_kernel_bias_loop67_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[67][1],
		(uint8_t *)(fd_kernel_bias_loop67_1_frame01),
		sizeof(fd_kernel_bias_loop67_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[68][0],
		(uint8_t *)(fd_kernel_bias_loop68_0_frame01),
		sizeof(fd_kernel_bias_loop68_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[68][1],
		(uint8_t *)(fd_kernel_bias_loop68_1_frame01),
		sizeof(fd_kernel_bias_loop68_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[69][0],
		(uint8_t *)(fd_kernel_bias_loop69_0_frame01),
		sizeof(fd_kernel_bias_loop69_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[69][1],
		(uint8_t *)(fd_kernel_bias_loop69_1_frame01),
		sizeof(fd_kernel_bias_loop69_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[70][0],
		(uint8_t *)(fd_kernel_bias_loop70_0_frame01),
		sizeof(fd_kernel_bias_loop70_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[70][1],
		(uint8_t *)(fd_kernel_bias_loop70_1_frame01),
		sizeof(fd_kernel_bias_loop70_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[71][0],
		(uint8_t *)(fd_kernel_bias_loop71_0_frame01),
		sizeof(fd_kernel_bias_loop71_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[71][1],
		(uint8_t *)(fd_kernel_bias_loop71_1_frame01),
		sizeof(fd_kernel_bias_loop71_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[72][0],
		(uint8_t *)(fd_kernel_bias_loop72_0_frame01),
		sizeof(fd_kernel_bias_loop72_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[72][1],
		(uint8_t *)(fd_kernel_bias_loop72_1_frame01),
		sizeof(fd_kernel_bias_loop72_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[73][0],
		(uint8_t *)(fd_kernel_bias_loop73_0_frame01),
		sizeof(fd_kernel_bias_loop73_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[73][1],
		(uint8_t *)(fd_kernel_bias_loop73_1_frame01),
		sizeof(fd_kernel_bias_loop73_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[74][0],
		(uint8_t *)(fd_kernel_bias_loop74_0_frame01),
		sizeof(fd_kernel_bias_loop74_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[74][1],
		(uint8_t *)(fd_kernel_bias_loop74_1_frame01),
		sizeof(fd_kernel_bias_loop74_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[75][0],
		(uint8_t *)(fd_kernel_bias_loop75_0_frame01),
		sizeof(fd_kernel_bias_loop75_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[75][1],
		(uint8_t *)(fd_kernel_bias_loop75_1_frame01),
		sizeof(fd_kernel_bias_loop75_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[76][0],
		(uint8_t *)(fd_kernel_bias_loop76_0_frame01),
		sizeof(fd_kernel_bias_loop76_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[76][1],
		(uint8_t *)(fd_kernel_bias_loop76_1_frame01),
		sizeof(fd_kernel_bias_loop76_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[77][0],
		(uint8_t *)(fd_kernel_bias_loop77_0_frame01),
		sizeof(fd_kernel_bias_loop77_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[77][1],
		(uint8_t *)(fd_kernel_bias_loop77_1_frame01),
		sizeof(fd_kernel_bias_loop77_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[78][0],
		(uint8_t *)(fd_kernel_bias_loop78_0_frame01),
		sizeof(fd_kernel_bias_loop78_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[78][1],
		(uint8_t *)(fd_kernel_bias_loop78_1_frame01),
		sizeof(fd_kernel_bias_loop78_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[79][0],
		(uint8_t *)(fd_kernel_bias_loop79_0_frame01),
		sizeof(fd_kernel_bias_loop79_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[79][1],
		(uint8_t *)(fd_kernel_bias_loop79_1_frame01),
		sizeof(fd_kernel_bias_loop79_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[80][0],
		(uint8_t *)(fd_kernel_bias_loop80_0_frame01),
		sizeof(fd_kernel_bias_loop80_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[80][1],
		(uint8_t *)(fd_kernel_bias_loop80_1_frame01),
		sizeof(fd_kernel_bias_loop80_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[81][0],
		(uint8_t *)(fd_kernel_bias_loop81_0_frame01),
		sizeof(fd_kernel_bias_loop81_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[81][1],
		(uint8_t *)(fd_kernel_bias_loop81_1_frame01),
		sizeof(fd_kernel_bias_loop81_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[82][0],
		(uint8_t *)(fd_kernel_bias_loop82_0_frame01),
		sizeof(fd_kernel_bias_loop82_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[82][1],
		(uint8_t *)(fd_kernel_bias_loop82_1_frame01),
		sizeof(fd_kernel_bias_loop82_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[83][0],
		(uint8_t *)(fd_kernel_bias_loop83_0_frame01),
		sizeof(fd_kernel_bias_loop83_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[83][1],
		(uint8_t *)(fd_kernel_bias_loop83_1_frame01),
		sizeof(fd_kernel_bias_loop83_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[84][0],
		(uint8_t *)(fd_kernel_bias_loop84_0_frame01),
		sizeof(fd_kernel_bias_loop84_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[84][1],
		(uint8_t *)(fd_kernel_bias_loop84_1_frame01),
		sizeof(fd_kernel_bias_loop84_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[85][0],
		(uint8_t *)(fd_kernel_bias_loop85_0_frame01),
		sizeof(fd_kernel_bias_loop85_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[85][1],
		(uint8_t *)(fd_kernel_bias_loop85_1_frame01),
		sizeof(fd_kernel_bias_loop85_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[86][0],
		(uint8_t *)(fd_kernel_bias_loop86_0_frame01),
		sizeof(fd_kernel_bias_loop86_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[86][1],
		(uint8_t *)(fd_kernel_bias_loop86_1_frame01),
		sizeof(fd_kernel_bias_loop86_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[87][0],
		(uint8_t *)(fd_kernel_bias_loop87_0_frame01),
		sizeof(fd_kernel_bias_loop87_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[87][1],
		(uint8_t *)(fd_kernel_bias_loop87_1_frame01),
		sizeof(fd_kernel_bias_loop87_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[88][0],
		(uint8_t *)(fd_kernel_bias_loop88_0_frame01),
		sizeof(fd_kernel_bias_loop88_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[88][1],
		(uint8_t *)(fd_kernel_bias_loop88_1_frame01),
		sizeof(fd_kernel_bias_loop88_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[89][0],
		(uint8_t *)(fd_kernel_bias_loop89_0_frame01),
		sizeof(fd_kernel_bias_loop89_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[89][1],
		(uint8_t *)(fd_kernel_bias_loop89_1_frame01),
		sizeof(fd_kernel_bias_loop89_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[90][0],
		(uint8_t *)(fd_kernel_bias_loop90_0_frame01),
		sizeof(fd_kernel_bias_loop90_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[90][1],
		(uint8_t *)(fd_kernel_bias_loop90_1_frame01),
		sizeof(fd_kernel_bias_loop90_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[91][0],
		(uint8_t *)(fd_kernel_bias_loop91_0_frame01),
		sizeof(fd_kernel_bias_loop91_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[91][1],
		(uint8_t *)(fd_kernel_bias_loop91_1_frame01),
		sizeof(fd_kernel_bias_loop91_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[92][0],
		(uint8_t *)(fd_kernel_bias_loop92_0_frame01),
		sizeof(fd_kernel_bias_loop92_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[92][1],
		(uint8_t *)(fd_kernel_bias_loop92_1_frame01),
		sizeof(fd_kernel_bias_loop92_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[93][0],
		(uint8_t *)(fd_kernel_bias_loop93_0_frame01),
		sizeof(fd_kernel_bias_loop93_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[93][1],
		(uint8_t *)(fd_kernel_bias_loop93_1_frame01),
		sizeof(fd_kernel_bias_loop93_1_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[94][0],
		(uint8_t *)(fd_kernel_bias_loop94_0_frame01),
		sizeof(fd_kernel_bias_loop94_0_frame01));
		fdvt_memcpy_ops((uint8_t *)g_FdDrv_Fd_DMA_Para->fd_kernel_PKVM_PA[94][1],
		(uint8_t *)(fd_kernel_bias_loop94_1_frame01),
		sizeof(fd_kernel_bias_loop94_1_frame01));
	}
}


void FDVT_updateConfig(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData)
{
	int crop_width = 0;
	int crop_height = 0;

	crop_width = FDVT_MetaData->srcImgWidth;
	crop_height = FDVT_MetaData->srcImgHeight;

	if (FDVT_MetaData->enROI) {
		crop_width = FDVT_MetaData->src_roi.x2 - FDVT_MetaData->src_roi.x1 + 1;
		crop_height = FDVT_MetaData->src_roi.y2 - FDVT_MetaData->src_roi.y1 + 1;
		if (crop_width == 0 || crop_height == 0)
			CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "src_crop_w is 0");
	}

	if (FDVT_MetaData->enPadding) {
		crop_width = crop_width + FDVT_MetaData->src_padding.right +
		FDVT_MetaData->src_padding.left;
		crop_height = crop_height + FDVT_MetaData->src_padding.up +
		FDVT_MetaData->src_padding.down;
	}

	if (FDVT_MetaData->FDMode == 0) {
		g_FdDrv_Para->FD_MODE = FDVT_MetaData->FDMode;
		g_FdDrv_Para->SRC_Crop_Width = crop_width;
		g_FdDrv_Para->SRC_Crop_Height = crop_height;
		g_FdDrv_Para->source_img_address = FDVT_MetaData->ImgSrcY_IOVA;
		g_FdDrv_Para->source_img_address_UV = FDVT_MetaData->ImgSrcUV_IOVA;
		g_FdDrv_Para->SRC_Input_Width =  FDVT_MetaData->srcImgWidth;
		g_FdDrv_Para->SRC_Input_Height = FDVT_MetaData->srcImgHeight;
		g_FdDrv_Para->SRC_IMG_FMT = FDVT_MetaData->srcImgFmt;
		g_FdDrv_Para->INPUT_ROTATE_DEGREE = FDVT_MetaData->rotateDegree;
		g_FdDrv_Para->pyramid_width = FDVT_MetaData->pyramid_width;
		g_FdDrv_Para->pyramid_height = FDVT_MetaData->pyramid_height;
		g_FdDrv_Para->RPN_ANCHOR_THRD = FDVT_MetaData->featureTH;
	}

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Crop_width = ");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)crop_width);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ", Crop_height = ");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)crop_height);

}

void FDVT_configY2R(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData,
	struct isp_meta_fd *FDVT_ExecMeta)
{
	uint64_t *image_buffer_address =
			(uint64_t *)(uintptr_t)FDVT_MetaData->ImgSrcY_IOVA;

	uint64_t *image_buffer_address_UV =
			(uint64_t *)(uintptr_t)FDVT_MetaData->ImgSrcUV_IOVA;

	uint64_t *yuv2rgb_cfg = NULL;
	//uint64_t srcbuf_64 = 0x0, srcbuf_UV_64 = 0x0;
	uint32_t srcbuf = 0x0, srcbuf_UV = 0x0;
	uint16_t xmag_0 = 0x0, ymag_0 = 0x0;
	uint16_t pyramid0_out_w = 0x0;
	uint16_t pyramid0_out_h = 0x0;
	uint16_t src_crop_w = 0x0;
	uint16_t src_crop_h = 0x0;

	if (FDVT_MetaData->enROI == false) {
		image_buffer_address = (uint64_t *)(uintptr_t)FDVT_MetaData->ImgSrcY_IOVA;
		image_buffer_address_UV = (uint64_t *)(uintptr_t)FDVT_MetaData->ImgSrcUV_IOVA;
	} else {
		if (FDVT_MetaData->srcImgFmt == FMT_MONO) {
			image_buffer_address    = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcY_IOVA    + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1);
			image_buffer_address_UV = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcUV_IOVA + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1);
		} else if (FDVT_MetaData->srcImgFmt == FMT_YUV_2P ||
				 FDVT_MetaData->srcImgFmt == FMT_YVU_2P) {
			image_buffer_address    = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcY_IOVA    + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1);
			image_buffer_address_UV = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcUV_IOVA + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1);
		} else if (FDVT_MetaData->srcImgFmt == FMT_YUYV ||
				 FDVT_MetaData->srcImgFmt == FMT_YVYU ||
				 FDVT_MetaData->srcImgFmt == FMT_UYVY ||
				 FDVT_MetaData->srcImgFmt == FMT_VYUY) {
			image_buffer_address    = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcY_IOVA    + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1 * 2);
			image_buffer_address_UV = (uint64_t *)((uint8_t *)(uintptr_t
			)FDVT_MetaData->ImgSrcUV_IOVA + (FDVT_MetaData->SRC_IMG_STRIDE *
			(FDVT_MetaData->src_roi).y1) + (FDVT_MetaData->src_roi).x1 * 2);
		} else {
			CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Unsupport input format:");
			CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->srcImgFmt);
		}
	}

	srcbuf = (uint32_t)(uintptr_t)image_buffer_address;

	if (FDVT_MetaData->srcImgFmt < 3 )
		srcbuf_UV = (uint32_t)(uintptr_t)image_buffer_address_UV;

	if (FDVT_MetaData->FDMode == 0) {
		src_crop_w = g_FdDrv_Para->SRC_Crop_Width;
		src_crop_h = g_FdDrv_Para->SRC_Crop_Height;
		yuv2rgb_cfg  = g_FdDrv_Para->FDMODE_YUV2RGB_Config_VA;
		pyramid0_out_w = g_FdDrv_Para->pyramid_width;
	}

	if (src_crop_w != 0)
		pyramid0_out_h = pyramid0_out_w * src_crop_h / src_crop_w;
	else
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "src_crop_w is 0");

	if (pyramid0_out_w != 0) {
		xmag_0 = 512 * src_crop_w / pyramid0_out_w;
		ymag_0 = xmag_0;
	}
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Y2R_config src_crop_w:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)src_crop_w);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ", src_crop_h:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)src_crop_h);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Y2R_config pyramid0_out_w:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)pyramid0_out_w);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ", pyramid0_out_h:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)pyramid0_out_h);

	if (FDVT_MetaData->FDMode == 0) {
#ifdef MODIFIED
		*((uint32_t *)yuv2rgb_cfg + Y2R_SRC_DST_FORMAT) = (*((uint32_t *)yuv2rgb_cfg +
		Y2R_SRC_DST_FORMAT) & 0xFFFFFFF8) | ((FDVT_MetaData->srcImgFmt) & 0x7);
		*((uint32_t *)yuv2rgb_cfg + Y2R_IN_W_H) = (*((uint32_t *)yuv2rgb_cfg +
		Y2R_IN_W_H) & 0xF800F800) | ((src_crop_w << 16) & 0x7FF0000) | (src_crop_h &
		0x7FF);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_W_H) = (*((uint32_t *)yuv2rgb_cfg +
		Y2R_OUT_W_H) & 0xF800F800) | ((pyramid0_out_w << 16) & 0x7FF0000) |
		(pyramid0_out_h & 0x7FF);

		if (FDVT_MetaData->srcImgFmt < 3) {
			*((uint32_t *)yuv2rgb_cfg + Y2R_RA0_RA1_EN) = (*((uint32_t *)yuv2rgb_cfg +
			Y2R_RA0_RA1_EN) & 0xFFFFFFEE) | 0x11; // RA_0_EN & RA_1_EN

			if (FDVT_MetaData->enROI == true) {
				CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enROI is true, reset width");
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) = ((FDVT_MetaData->src_roi.x2
				- FDVT_MetaData->src_roi.x1) & 0xFFFF) | (((FDVT_MetaData->src_roi.y2 -
				FDVT_MetaData->src_roi.y1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) = ((FDVT_MetaData->src_roi.x2
				- FDVT_MetaData->src_roi.x1) & 0xFFFF) | (((FDVT_MetaData->src_roi.y2 -
				FDVT_MetaData->src_roi.y1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
			} else {
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) = ((src_crop_w - 1) & 0xFFFF)
				| (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) = ((src_crop_w - 1) & 0xFFFF)
				| (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_1 & IN_YSIZE_1
			}
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x1;
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x1;
		} else if (FDVT_MetaData->srcImgFmt == FMT_MONO) {
			*((uint32_t *)yuv2rgb_cfg + Y2R_RA0_RA1_EN) = (*((uint32_t *)yuv2rgb_cfg +
			Y2R_RA0_RA1_EN) & 0xFFFFFFEE) | 0x01; // RA_0_EN & RA_1_EN

			if (FDVT_MetaData->enROI == true) {
				CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enROI is true, reset width");
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) = ((FDVT_MetaData->src_roi.x2
				- FDVT_MetaData->src_roi.x1) & 0xFFFF) | (((FDVT_MetaData->src_roi.y2 -
				FDVT_MetaData->src_roi.y1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) = ((FDVT_MetaData->src_roi.x2
				- FDVT_MetaData->src_roi.x1) & 0xFFFF) | (((FDVT_MetaData->src_roi.y2 -
				FDVT_MetaData->src_roi.y1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
			} else {
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) = ((src_crop_w - 1) & 0xFFFF)
				| (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) = ((src_crop_w - 1) & 0xFFFF)
				| (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_1 & IN_YSIZE_1
			}
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x0;
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x0;
		} else {
			*((uint32_t *)yuv2rgb_cfg + Y2R_RA0_RA1_EN) = (*((uint32_t *)yuv2rgb_cfg +
			Y2R_RA0_RA1_EN) & 0xFFFFFFEE) | 0x1; // RA_0_EN

			if (FDVT_MetaData->enROI == true) {
				CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enROI is true, reset width");
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) =
				((2*(FDVT_MetaData->src_roi.x2 - FDVT_MetaData->src_roi.x1 + 1) - 1) &
				0xFFFF) | (((FDVT_MetaData->src_roi.y2 - FDVT_MetaData->src_roi.y1) << 16)
				& 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) =
				((2*(FDVT_MetaData->src_roi.x2 - FDVT_MetaData->src_roi.x1 + 1) - 1) &
				0xFFFF) | (((FDVT_MetaData->src_roi.y2 - FDVT_MetaData->src_roi.y1) << 16)
				& 0xFFFF0000); // IN_XSIZE_1 & IN_YSIZE_1
			} else{
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE0) = ((2*src_crop_w - 1) & 0xFFFF
				) | (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_0 & IN_YSIZE_0
				*((uint32_t *)yuv2rgb_cfg + Y2R_IN_X_Y_SIZE1) = ((2*src_crop_w - 1) & 0xFFFF
				) | (((src_crop_h - 1) << 16) & 0xFFFF0000); // IN_XSIZE_1 & IN_YSIZE_1
			}
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE0_BUS_SIZE0) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x3;
			*((uint32_t *)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) = (*((uint32_t *
			)yuv2rgb_cfg + Y2R_IN_STRIDE1_BUS_SIZE1) & 0xFFF0) |
			((FDVT_MetaData->SRC_IMG_STRIDE << 16) & 0xFFFF0000) | 0x3;
		}
#endif
#ifdef MODIFIED
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_X_Y_SIZE0) = ((pyramid0_out_w - 1) &
		0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_STRIDE0_BUS_SIZE0) = (*((uint32_t *
		)yuv2rgb_cfg + Y2R_OUT_STRIDE0_BUS_SIZE0) & 0xFFFF) | (((pyramid0_out_w) <<
		16) & 0xFFFF0000); // OUT_STRIDE_0
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_X_Y_SIZE1) = ((pyramid0_out_w - 1) &
		0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_STRIDE1_BUS_SIZE1) = (*((uint32_t *
		)yuv2rgb_cfg + Y2R_OUT_STRIDE1_BUS_SIZE1) & 0xFFFF) | (((pyramid0_out_w) <<
		16) & 0xFFFF0000); // OUT_STRIDE_1
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_X_Y_SIZE2) = ((pyramid0_out_w - 1) &
		0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_STRIDE2_BUS_SIZE2) = (*((uint32_t *
		)yuv2rgb_cfg + Y2R_OUT_STRIDE2_BUS_SIZE2) & 0xFFFF) | (((pyramid0_out_w) <<
		16) & 0xFFFF0000); // OUT_STRIDE_2

		if (FDVT_MetaData->enPadding == true) {
			CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enPadding is true");
			*((uint32_t *)yuv2rgb_cfg + Y2R_PADDING_EN_UP_DOWN) = (1 & 0x0001) |
			((FDVT_MetaData->src_padding.up << 4) & 0x1FF0) |
			((FDVT_MetaData->src_padding.down << 16) & 0x01FF0000);
			*((uint32_t *)yuv2rgb_cfg + Y2R_PADDING_RIGHT_LEFT) =
			(FDVT_MetaData->src_padding.right & 0x01FF) |
			((FDVT_MetaData->src_padding.left << 16) & 0x01FF0000);
		} else {
			*((uint32_t *)yuv2rgb_cfg + Y2R_PADDING_EN_UP_DOWN) = 0;
			*((uint32_t *)yuv2rgb_cfg + Y2R_PADDING_RIGHT_LEFT) = 0;
		}
#endif
		*((uint32_t *)yuv2rgb_cfg + Y2R_IN_0) = (uint32_t)srcbuf;
		*((uint32_t *)yuv2rgb_cfg + Y2R_IN_1) = (uint32_t)srcbuf_UV;

		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_0) = (uint32_t)((uintptr_t
		)g_FdDrv_Para->RS_Pyramid0_R_Result_PA);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_1) = (uint32_t)((uintptr_t
		)g_FdDrv_Para->RS_Pyramid0_G_Result_PA);
		*((uint32_t *)yuv2rgb_cfg + Y2R_OUT_2) = (uint32_t)((uintptr_t
		)g_FdDrv_Para->RS_Pyramid0_B_Result_PA);
#ifdef MODIFIED

		*((uint32_t *)yuv2rgb_cfg + Y2R_X_Y_MAG) = (xmag_0 & 0x3FFF) | ((ymag_0 << 16)
		& 0x3FFF0000); // X_MAG & Y_MAG

		if (src_crop_w >= pyramid0_out_w) {
			*((uint32_t *)yuv2rgb_cfg + Y2R_RS_SEL_SRZ_EN) = (*((uint32_t *)yuv2rgb_cfg +
			Y2R_RS_SEL_SRZ_EN) & 0x00100070) | (0x0 << 16) | (0x0 << 12) | (0x0 << 8) |
			0x0;
			*((uint32_t *)yuv2rgb_cfg + Y2R_SRZ_HORI_STEP) = 0;
			*((uint32_t *)yuv2rgb_cfg + Y2R_SRZ_VERT_STEP) = 0;
		} else {
			//0: FDRZ for down scaling
			//1: SRZ for up scaling
			*((uint32_t *)yuv2rgb_cfg + Y2R_RS_SEL_SRZ_EN) = (*((uint32_t *)yuv2rgb_cfg +
			Y2R_RS_SEL_SRZ_EN) & 0x00100070) | (0x1 << 16) | (0x1 << 12) | (0x1 << 8) |
			0x1;
			*((uint32_t *)yuv2rgb_cfg + Y2R_SRZ_HORI_STEP) = ((1 << 15) * (src_crop_w - 1
			)) / (pyramid0_out_w - 1);
			*((uint32_t *)yuv2rgb_cfg + Y2R_SRZ_VERT_STEP) = ((1 << 15) * (src_crop_h - 1
			)) / (pyramid0_out_h - 1);
		}
#endif
	}
}


void FDVT_configRS(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData)
{
	uint64_t *rs_cfg;
	uint16_t xmag_0 = 0x0, ymag_0 = 0x0;
	uint16_t pyramid0_out_w = 0x0, pyramid1_out_w = 0x0, pyramid2_out_w = 0x0;
	uint16_t pyramid0_out_h = 0x0, pyramid1_out_h = 0x0, pyramid2_out_h = 0x0;
	uint16_t src_crop_w = 0x0;
	uint16_t src_crop_h = 0x0;


	if (FDVT_MetaData->FDMode == 0) {
		src_crop_w = g_FdDrv_Para->SRC_Crop_Width;
		src_crop_h = g_FdDrv_Para->SRC_Crop_Height;
	}

	rs_cfg  = g_FdDrv_Para->FDMODE_RS_Config_VA;

	pyramid0_out_w = g_FdDrv_Para->pyramid_width;
	pyramid1_out_w = pyramid0_out_w >> 1;
	pyramid2_out_w = pyramid1_out_w >> 1;

	if (src_crop_w != 0)
		pyramid0_out_h = pyramid0_out_w * src_crop_h / src_crop_w;
	else
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "src_crop_w is 0");

	pyramid1_out_h = pyramid0_out_h >> 1;
	pyramid2_out_h = pyramid1_out_h >> 1;

	// pyramid 1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid0_R_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid0_G_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid0_B_Result_PA);

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_R_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_G_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_B_Result_PA);

	// pyramid 2
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_R_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_G_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid1_B_Result_PA);

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid2_R_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid2_G_Result_PA);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Para->RS_Pyramid2_B_Result_PA);

#ifdef MODIFIED
	// pyramid 1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_INPUT_W_H) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_INPUT_W_H) & 0xF800F800) | (pyramid0_out_h &
	0x7FF) | ((pyramid0_out_w << 16) & 0x7FF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUTPUT_W_H) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUTPUT_W_H) & 0xF800F800) | (pyramid1_out_h
	& 0x7FF) | ((pyramid1_out_w << 16) & 0x7FF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_X_Y_SIZE0) = ((pyramid0_out_w
	- 1) & 0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_X_Y_SIZE1) = ((pyramid0_out_w
	- 1) & 0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_X_Y_SIZE2) = ((pyramid0_out_w
	- 1) & 0xFFFF) | (((pyramid0_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE0) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE0) & 0xFFFF) | ((pyramid0_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE1) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE1) & 0xFFFF) | ((pyramid0_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE2) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_IN_STRIDE2) & 0xFFFF) | ((pyramid0_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_2

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_X_Y_SIZE0) =
	((pyramid1_out_w - 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_0 & OUT_YSIZE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_X_Y_SIZE1) =
	((pyramid1_out_w - 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_1 & OUT_YSIZE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_X_Y_SIZE2) =
	((pyramid1_out_w - 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_2 & OUT_YSIZE_2

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE0) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE0) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // OUT_STRIDE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE1) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE1) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // OUT_STRIDE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE2) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 0 + RS_OUT_STRIDE2) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // OUT_STRIDE_2

	xmag_0 = 512 * pyramid0_out_w / pyramid1_out_w;
	ymag_0 = xmag_0;

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 0 + RS_X_Y_MAG) = (xmag_0 & 0x3FFF) |
	((ymag_0 << 16) & 0x3FFF0000); // X_MAG & Y_MAG

	// pyramid 2
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_INPUT_W_H) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_INPUT_W_H) & 0xF800F800) | (pyramid1_out_h &
	0x7FF) | ((pyramid1_out_w << 16) & 0x7FF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUTPUT_W_H) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUTPUT_W_H) & 0xF800F800) | (pyramid2_out_h
	& 0x7FF) | ((pyramid2_out_w << 16) & 0x7FF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_X_Y_SIZE0) = ((pyramid1_out_w
	- 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_X_Y_SIZE1) = ((pyramid1_out_w
	- 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_X_Y_SIZE2) = ((pyramid1_out_w
	- 1) & 0xFFFF) | (((pyramid1_out_h - 1) << 16) & 0xFFFF0000);
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE0) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE0) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE1) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE1) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE2) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_IN_STRIDE2) & 0xFFFF) | ((pyramid1_out_w <<
	16) & 0xFFFF0000); // IN_STRIDE_2

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_X_Y_SIZE0) =
	((pyramid2_out_w - 1) & 0xFFFF) | (((pyramid2_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_0 & OUT_YSIZE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_X_Y_SIZE1) =
	((pyramid2_out_w - 1) & 0xFFFF) | (((pyramid2_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_1 & OUT_YSIZE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_X_Y_SIZE2) =
	((pyramid2_out_w - 1) & 0xFFFF) | (((pyramid2_out_h - 1) << 16) & 0xFFFF0000);
	// OUT_XSIZE_2 & OUT_YSIZE_2

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE0) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE0) & 0xFFFF) |
	(((((pyramid2_out_w+7)/8)*8) << 16) & 0xFFFF0000); // OUT_STRIDE_0
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE1) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE1) & 0xFFFF) |
	(((((pyramid2_out_w+7)/8)*8) << 16) & 0xFFFF0000); // OUT_STRIDE_1
	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE2) = (*((uint32_t *
	)rs_cfg + RS_CONFIG_SIZE * 1 + RS_OUT_STRIDE2) & 0xFFFF) |
	(((((pyramid2_out_w+7)/8)*8) << 16) & 0xFFFF0000); // OUT_STRIDE_2

	xmag_0 = 512 * pyramid1_out_w / pyramid2_out_w;
	ymag_0 = xmag_0;

	*((uint32_t *)rs_cfg + RS_CONFIG_SIZE * 1 + RS_X_Y_MAG) = (xmag_0 & 0x3FFF) |
	((ymag_0 << 16) & 0x3FFF0000); // X_MAG & Y_MAG
#endif
}

void FDVT_configNetwork(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData)
{
	uint16_t conv_width = 0x0;
	uint16_t conv_height = 0x0;
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t uesd_out_ch = 0;
	uint8_t uesd_out_loop = 0;
	uint16_t fd_xsize0 = 0;
	uint16_t fd_xsize1 = 0;
	uint16_t fd_xsize2 = 0;
	uint16_t fd_xsize3 = 0;
	uint64_t *fd_cfg;
	uint16_t pyramid0_out_w = 0x0;
	uint16_t pyramid0_out_h = 0x0;
	//uint16_t pyramid1_out_w = 0x0;
	uint16_t pyramid1_out_h = 0x0;
	//uint16_t pyramid2_out_w = 0x0;
	uint16_t pyramid2_out_h = 0x0;
	uint16_t input_height = 0;
	uint16_t out_height = 0;
	uint16_t out_ysize_plus_1 = 0;
	uint16_t out_ysize_plus_1_stride2 = 0;
	uint16_t src_crop_w = 0x0;
	uint16_t src_crop_h = 0x0;

	if (FDVT_MetaData->FDMode == 0) {
		src_crop_w = g_FdDrv_Para->SRC_Crop_Width;
		src_crop_h = g_FdDrv_Para->SRC_Crop_Height;
	}

	pyramid0_out_w = g_FdDrv_Para->pyramid_width;
	//pyramid1_out_w = pyramid0_out_w / 2;
	//pyramid2_out_w = pyramid1_out_w / 2;

	if (src_crop_w != 0)
		pyramid0_out_h = pyramid0_out_w * src_crop_h / src_crop_w;
	else
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "src_crop_w is 0");

	pyramid1_out_h = pyramid0_out_h / 2;
	pyramid2_out_h = pyramid1_out_h / 2;

	fd_cfg  = (uint64_t *)&g_FD_Config[0];

	for (i = 0; i < fdvt_fd_loop_num; i++) {
#ifdef MODIFIED
		*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_INPUT_ROTATE) = (*((uint32_t *
		)fd_cfg + FD_CONFIG_SIZE * i + FD_INPUT_ROTATE) & 0xFFFF0FFF) |
		((FDVT_MetaData->rotateDegree << 12) & 0x3000); // INPUT_ROTATE

		if (i == 0)
			input_height = pyramid2_out_h;
		else if (i == (fdvt_fd_rpn2_loop_num + 1))
			input_height = pyramid1_out_h;
		else if (i == (fdvt_fd_rpn1_loop_num + 1))
			input_height = pyramid0_out_h;
		else {
			if (Used_Output_Stride2_as_Input[i] == 0)
				input_height = out_height;
			else if (Used_Output_Stride2_as_Input[i] == 1)
				input_height = (out_height + 1) / 2; // ceiling
		}
		if (i == fdvt_fd_rpn0_loop_num)
			gFD_pose_height = input_height;

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1)
			out_height = (input_height + (2 * fd_maxpool[i] - 1)) / (2 * fd_maxpool[i]);
			// ceiling
		else
			out_height = (input_height + (fd_stride[i] + 2 * fd_maxpool[i] - 1)) /
			(fd_stride[i] + 2 * fd_maxpool[i]); // ceiling

		if (i == fdvt_fd_rpn0_loop_num || i == fdvt_fd_rpn1_loop_num || i ==
		fdvt_fd_rpn2_loop_num) {
			//conv_width = src_crop_w;
			//conv_height = src_crop_h;
			conv_width = 0x7ff;
			conv_height = 0x7ff;
		} else {
			conv_width = (image_width[i] + (1 * fd_stride[i] - 1)) / (1 * fd_stride[i]);
			// ceiling
			conv_height = (input_height + (1 * fd_stride[i] - 1)) / (1 * fd_stride[i]);
			// ceiling
		}

		*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_CONV_WIDTH_MOD6) = (*((uint32_t
		*)fd_cfg + FD_CONFIG_SIZE * i + FD_CONV_WIDTH_MOD6) & 0xFF8FFFFF) |
		(((conv_width % 6) << 20) & 0x00700000);
		*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_CONV_IMG_W_H) = ((conv_width <<
		16) & 0xFFFF0000) | (conv_height & 0xFFFF);

		*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_IMG_W_H) = ((image_width[i]
		<< 16) & 0xFFFF0000) | (input_height & 0xFFFF);
		*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_IMG_W_H) =
		((output_width[i] << 16) & 0xFFFF0000) | (out_height & 0xFFFF);

		if (i == fdvt_fd_rpn0_loop_num || i == fdvt_fd_rpn1_loop_num || i ==
		fdvt_fd_rpn2_loop_num) {
			fd_xsize0 = (image_width[i] * 2 * 16 * anchor_enable_number[i]) - 1;
			fd_xsize1 = fd_xsize2 = fd_xsize3 = (image_width[i] * 2 * 32 *
			anchor_enable_number[i]) - 1;
		} else {
			fd_xsize0 = fd_xsize1 = fd_xsize2 = fd_xsize3 = input_xsize_plus_1[i] - 1;
		}

		if (input_RDMA_RA_buffer_en[i][0][0] != -1) {
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_X_Y_SIZE0) = ((fd_xsize0) &
			0xFFFF) | (((input_height - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_0
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_X_Y_SIZE1) = ((fd_xsize1) &
			0xFFFF) | (((input_height - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_1
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_X_Y_SIZE2) = ((fd_xsize2) &
			0xFFFF) | (((input_height - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_2
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_X_Y_SIZE3) = ((fd_xsize3) &
			0xFFFF) | (((input_height - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_3

			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE0_BUS_SIZE0) =
			(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE0_BUS_SIZE0) &
			0x000F) | (((fd_xsize0 + 1) << 16) & 0xFFFF0000);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE1_BUS_SIZE1) =
			(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE1_BUS_SIZE1) &
			0x000F) | (((fd_xsize1 + 1) << 16) & 0xFFFF0000);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE2_BUS_SIZE2) =
			(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE2_BUS_SIZE2) &
			0x000F) | (((fd_xsize2 + 1) << 16) & 0xFFFF0000);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE3_BUS_SIZE3) =
			(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_STRIDE3_BUS_SIZE3) &
			0x000F) | (((fd_xsize3 + 1) << 16) & 0xFFFF0000);
		}

		out_ysize_plus_1 = out_height - 1;
		out_ysize_plus_1_stride2 = (out_height + 1)/2 - 1;

		for (j = 0; j < output_WDMA_WRA_num; j++) {
			if (output_WDMA_WRA_buffer_en[i][j]) {
				if (out_stride_size[i][j] == 1) {
					*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_X_Y_SIZE0 + 2 * j) =
					((out_xsize_plus_1[i] - 1) & 0xFFFF) | ((out_ysize_plus_1 << 16) &
					0xFFFF0000); // OUTPUT_YSIZE_0
					*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_STRIDE0_BUS_SIZE0 + 2 *
					j) = (*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_STRIDE0_BUS_SIZE0
					+ 2 * j) & 0x000F) | ((out_stride[i] << 16) & 0xFFFF0000);
				} else if (out_stride_size[i][j] == 2) {
					*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_X_Y_SIZE0 + 2 * j) =
					((out_xsize_plus_1_stride2[i] - 1) & 0xFFFF) | ((out_ysize_plus_1_stride2
					<< 16) & 0xFFFF0000); // OUTPUT_YSIZE_0
					*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_STRIDE0_BUS_SIZE0 + 2 *
					j) = (*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_OUT_STRIDE0_BUS_SIZE0
					+ 2 * j) & 0x000F) | ((out_stride_stride2[i] << 16) & 0xFFFF0000);
				}
			}
		}

		if (i == fdvt_fd_rpn0_loop_num || i == fdvt_fd_rpn1_loop_num || i ==
		fdvt_fd_rpn2_loop_num) {
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_RPN_SET) = (*((uint32_t *
			)fd_cfg + FD_CONFIG_SIZE * i + FD_RPN_SET) & 0xFFFF) |
			((g_FdDrv_Para->RPN_ANCHOR_THRD << 16) & 0xFFFF0000); // RPN_ANCHOR_THRD
		}

		int scale = 0;

		if (i == fdvt_fd_rpn0_loop_num) {
			scale = ((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 512) / 100;
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) = (*((uint32_t *
			)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
			0x7FFF0); // IMAGE_COORD_SCALE
		} else if (i == fdvt_fd_rpn1_loop_num) {
			scale = ((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 2 * 512) /
			100;
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) = (*((uint32_t *
			)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
			0x7FFF0); // IMAGE_COORD_SCALE
		} else if (i == fdvt_fd_rpn2_loop_num) {
			scale = ((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 4 * 512) /
			100;
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) = (*((uint32_t *
			)fd_cfg + FD_CONFIG_SIZE * i + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
			0x7FFF0); // IMAGE_COORD_SCALE
		}
#endif

		// IN_FM_BASE_ADR
		if (i == 0) {
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_0) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid2_R_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_1) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid2_G_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_2) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid2_B_Result_PA);
		} else if (i == (fdvt_fd_rpn2_loop_num + 1)) {
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_0) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid1_R_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_1) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid1_G_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_2) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid1_B_Result_PA);
		} else if (i == (fdvt_fd_rpn1_loop_num + 1)) {
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_0) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid0_R_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_1) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid0_G_Result_PA);
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + FD_IN_2) = (uint32_t)((uintptr_t
			)g_FdDrv_Para->RS_Pyramid0_B_Result_PA);
		} else {
			for (j = 0; j < input_WDMA_WRA_num ; j++) {
				if (input_RDMA_RA_buffer_en[i][j][0] != -1) {
					uesd_out_loop = input_RDMA_RA_buffer_en[i][j][0];
					uesd_out_ch = input_RDMA_RA_buffer_en[i][j][1];
			*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + (FD_IN_0 + j)) =
	(uint32_t)((uintptr_t)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[uesd_out_loop][uesd_out_ch]);
				}
			}
		}


		// OUT_FM_BASE_ADR
		for (j = 0; j < output_WDMA_WRA_num ; j++) {
			if (output_WDMA_WRA_buffer_en[i][j] == 1) {
				*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + (FD_OUT_0 + j)) = (uint32_t
				)((uintptr_t)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[i][j]);
			}
		}

		// KERNEL_BASE_ADR
		for (j = 0; j < kernel_RDMA_RA_num ; j++) {
			if (kernel_RDMA_RA_buffer_en[i][j] == 1) {
				*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * i + (FD_KERNEL_0 + j)) = (uint32_t
				)((uintptr_t)g_FdDrv_Fd_DMA_Para->fd_kernel_PA[i][j]);
			}
		}
	}
}

void FDVT_configExtendedNetwork(struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData)
{
	uint16_t fd_xsize0 = 0;
	uint16_t fd_xsize1 = 0;
	uint16_t fd_xsize2 = 0;
	uint16_t fd_xsize3 = 0;
	uint16_t src_crop_w = 0;
	//uint16_t src_crop_h = 0;
	uint16_t rpn2_in_w = 0;
	uint16_t rpn2_in_h = 0;
	uint16_t rpn1_in_w = 0;
	uint16_t rpn1_in_h = 0;
	uint16_t rpn0_in_w = 0;
	uint16_t rpn0_in_h = 0;
	uint64_t *fd_cfg = NULL;

	fd_cfg  = g_FdDrv_Para->FDMODE_FD_POSE_Config_VA;

	src_crop_w = g_FdDrv_Para->SRC_Crop_Width;
	//src_crop_h = g_FdDrv_Para->SRC_Crop_Height;

	rpn2_in_w = image_width[fdvt_fd_rpn0_loop_num];
	rpn2_in_h = gFD_pose_height;
	rpn1_in_w = ((rpn2_in_w + 1) / 2);
	rpn1_in_h = ((rpn2_in_h + 1) / 2);
	rpn0_in_w = ((rpn1_in_w + 1) / 2);
	rpn0_in_h = ((rpn1_in_h + 1) / 2);

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_INPUT_ROTATE) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 0 + FD_INPUT_ROTATE) & 0xFFFF0FFF) |
	((FDVT_MetaData->rotateDegree << 12) & 0x3000); // INPUT_ROTATE
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_INPUT_ROTATE) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 1 + FD_INPUT_ROTATE) & 0xFFFF0FFF) |
	((FDVT_MetaData->rotateDegree << 12) & 0x3000); // INPUT_ROTATE
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_INPUT_ROTATE) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 2 + FD_INPUT_ROTATE) & 0xFFFF0FFF) |
	((FDVT_MetaData->rotateDegree << 12) & 0x3000); // INPUT_ROTATE

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[22][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[19][1]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_3) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[28][0]);

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[54][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[51][1]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_3) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[60][0]);

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[86][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_1) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_2) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[83][1]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_3) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_out_hw_PA[92][0]);

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_OUT_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_PA[0][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_OUT_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_PA[1][0]);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_OUT_0) = (uint32_t)((uintptr_t
	)g_FdDrv_Fd_DMA_Para->fd_pose_out_hw_PA[2][0]);

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_IMG_W_H) = ((rpn0_in_w << 16)
	& 0xFFFF0000) | (rpn0_in_h & 0xFFFF); // INPUT_IMG_HT
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_OUT_IMG_W_H) = ((rpn0_in_w << 16
	) & 0xFFFF0000) | (rpn0_in_h & 0xFFFF); // OUTPUT_IMG_HT
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_IMG_W_H) = ((rpn1_in_w << 16)
	& 0xFFFF0000) | (rpn1_in_h  & 0xFFFF); // INPUT_IMG_HT
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_OUT_IMG_W_H) = ((rpn1_in_w << 16
	) & 0xFFFF0000) | (rpn1_in_h & 0xFFFF); // OUTPUT_IMG_HT
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_IMG_W_H) = ((rpn2_in_w << 16)
	& 0xFFFF0000) | (rpn2_in_h & 0xFFFF); // INPUT_IMG_HT
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_OUT_IMG_W_H) = ((rpn2_in_w << 16
	) & 0xFFFF0000) | (rpn2_in_h & 0xFFFF); // OUTPUT_IMG_HT

	fd_xsize0 = (rpn0_in_w * 2 * 16 * 5) - 1;
	fd_xsize1 = fd_xsize2 = fd_xsize3 = (rpn0_in_w * 2 * 32 * 5) - 1;

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_X_Y_SIZE0) = ((fd_xsize0) &
	0xFFFF) | (((rpn0_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_0
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_X_Y_SIZE1) = ((fd_xsize1) &
	0xFFFF) | (((rpn0_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_1
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_X_Y_SIZE2) = ((fd_xsize2) &
	0xFFFF) | (((rpn0_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_2
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_X_Y_SIZE3) = ((fd_xsize3) &
	0xFFFF) | (((rpn0_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_3

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE0_BUS_SIZE0) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE0_BUS_SIZE0) & 0x000F)
	| (((fd_xsize0 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE1_BUS_SIZE1) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE1_BUS_SIZE1) & 0x000F)
	| (((fd_xsize1 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE2_BUS_SIZE2) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE2_BUS_SIZE2) & 0x000F)
	| (((fd_xsize2 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE3_BUS_SIZE3) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IN_STRIDE3_BUS_SIZE3) & 0x000F)
	| (((fd_xsize3 + 1) << 16) & 0xFFFF0000);

	fd_xsize0 = (rpn1_in_w * 2 * 16 * 5) - 1;
	fd_xsize1 = fd_xsize2 = fd_xsize3 = (rpn1_in_w * 2 * 32 * 5) - 1;

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_X_Y_SIZE0) = ((fd_xsize0) &
	0xFFFF) | (((rpn1_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_0
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_X_Y_SIZE1) = ((fd_xsize1) &
	0xFFFF) | (((rpn1_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_1
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_X_Y_SIZE2) = ((fd_xsize2) &
	0xFFFF) | (((rpn1_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_2
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_X_Y_SIZE3) = ((fd_xsize3) &
	0xFFFF) | (((rpn1_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_3

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE0_BUS_SIZE0) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE0_BUS_SIZE0) & 0x000F)
	| (((fd_xsize0 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE1_BUS_SIZE1) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE1_BUS_SIZE1) & 0x000F)
	| (((fd_xsize1 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE2_BUS_SIZE2) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE2_BUS_SIZE2) & 0x000F)
	| (((fd_xsize2 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE3_BUS_SIZE3) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IN_STRIDE3_BUS_SIZE3) & 0x000F)
	| (((fd_xsize3 + 1) << 16) & 0xFFFF0000);

	fd_xsize0 = (rpn2_in_w * 2 * 16 * 5) - 1;
	fd_xsize1 = fd_xsize2 = fd_xsize3 = (rpn2_in_w * 2 * 32 * 5) - 1;

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_X_Y_SIZE0) = ((fd_xsize0) &
	0xFFFF) | (((rpn2_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_0
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_X_Y_SIZE1) = ((fd_xsize1) &
	0xFFFF) | (((rpn2_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_1
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_X_Y_SIZE2) = ((fd_xsize2) &
	0xFFFF) | (((rpn2_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_2
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_X_Y_SIZE3) = ((fd_xsize3) &
	0xFFFF) | (((rpn2_in_h - 1) << 16) & 0xFFFF0000); // INPUT_YSIZE_3

	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE0_BUS_SIZE0) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE0_BUS_SIZE0) & 0x000F)
	| (((fd_xsize0 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE1_BUS_SIZE1) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE1_BUS_SIZE1) & 0x000F)
	| (((fd_xsize1 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE2_BUS_SIZE2) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE2_BUS_SIZE2) & 0x000F)
	| (((fd_xsize2 + 1) << 16) & 0xFFFF0000);
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE3_BUS_SIZE3) =
	(*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IN_STRIDE3_BUS_SIZE3) & 0x000F)
	| (((fd_xsize3 + 1) << 16) & 0xFFFF0000);

	int scale = 0;

	scale = (int)((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 512) /
	100;
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IMAGE_COORD) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 2 + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
	0x7FFF0); // IMAGE_COORD_SCALE
	scale = (int)((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 2 * 512)
	/ 100;
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IMAGE_COORD) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 1 + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
	0x7FFF0); // IMAGE_COORD_SCALE
	scale = (int)((int)(src_crop_w * 100 / g_FdDrv_Para->pyramid_width) * 4 * 512)
	/ 100;
	*((uint32_t *)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IMAGE_COORD) = (*((uint32_t *
	)fd_cfg + FD_CONFIG_SIZE * 0 + FD_IMAGE_COORD) & 0xF) | ((scale << 4) &
	0x7FFF0); // IMAGE_COORD_SCALE


}

void cmdq_set_fdvt_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_fdvt_ops = ops;
	CALL_FROM_CMDQ_FDVT_OPS(puts, __func__);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enter");
}

int32_t cmdq_drv_isp_setup_task_fd(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	uint32_t *YUVConfig_VA;
	uint32_t *RSConfig_VA;
#ifndef OUTPUT_USE_NORMAL_BUFFER
	uint32_t *RSOutBuf_VA;
#endif
	uint32_t *FD_Pose_Config_VA;

	if (!data || !isp_execmeta|| !size) {
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT][err] data:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)data);
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT][err] isp_execmeta:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)isp_execmeta);
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT][err] size:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)size);
		return 0;
	}
	struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData;

	FDVT_MetaData = (struct FDVT_SEC_MetaDataToGCE *)(data);

	struct isp_meta_fd *FDVT_ExecMeta;

	FDVT_ExecMeta = (struct isp_meta_fd *)(&isp_execmeta->fd);
	g_FdDrv_Para = &Fd_Para;
	g_FdDrv_Fd_DMA_Para = &Fd_FD_DMA_Para;
	fdvt_frame_R_size = (unsigned int)(FDVT_MetaData->maxWidth *
	FDVT_MetaData->maxHeight);
	fdvt_frame_G_size = (unsigned int)fdvt_frame_R_size;
	fdvt_frame_B_size = (unsigned int)fdvt_frame_R_size;
	fdvt_rs_pyramid0_out_size = (unsigned int)(FDVT_MetaData->pyramid_width *
	FDVT_MetaData->pyramid_height);
	fdvt_rs_pyramid1_out_size = fdvt_rs_pyramid0_out_size / 2;
	fdvt_rs_pyramid2_out_size = fdvt_rs_pyramid1_out_size / 2 ;

	FDVT_initFdvtTable(FDVT_MetaData->pyramid_width, FDVT_MetaData->pyramid_height
	);
	FDVT_arrangeConfigAddress_PA(FDVT_MetaData, FDVT_ExecMeta);
	FDVT_arrangeOutputAddress_PA(FDVT_MetaData, FDVT_ExecMeta);
	FDVT_updateConfig(FDVT_MetaData);

	/* set iova */
	g_RSConfig_IOVA = FDVT_MetaData->RSConfig_IOVA;
	g_FDConfig_IOVA = FDVT_MetaData->FDConfig_IOVA;
	g_YUVConfig_IOVA = FDVT_MetaData->YUVConfig_IOVA;
	g_FDPOSE_IOVA = FDVT_MetaData->FDPOSE_IOVA;
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] ImgSrcY_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrcY_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] ImgSrcUV_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrcUV_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] YUVConfig_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->YUVConfig_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] YUVOutBuf_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->YUVOutBuf_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] RSConfig_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->RSConfig_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] RSOutBuf_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->RSOutBuf_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDConfig_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDConfig_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDOutBuf_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDOutBuf_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDPOSE_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDPOSE_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FD_POSE_Config_IOVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FD_POSE_Config_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDResultBuf_MVA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDResultBuf_MVA);

	#ifdef DRIVER_UT
	/* Y-Plane */
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] ImgSrcY_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrcY_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FDVT_MetaData->ImgSrc_Y_Size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrc_Y_Size);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "memcpy ImgSrcY_VA");
	fdvt_memcpy_ops((uint8_t *)(uintptr_t)(FDVT_MetaData->ImgSrcY_Handler),
	(uint8_t *)(uintptr_t)(fd_in1_0910_338x600_FMT_YVU_2P),
	sizeof(fd_in1_0910_338x600_FMT_YVU_2P));
	#endif

	/* UV-Plane */
	if (FDVT_MetaData->ImgSrcUV_Handler) {
		#ifdef DRIVER_UT
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] ImgSrcUV_PA:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrcUV_Handler);
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FDVT_MetaData->ImgSrc_UV_Size:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->ImgSrc_UV_Size);
		fdvt_memcpy_ops((uint8_t *)(uintptr_t)(FDVT_MetaData->ImgSrcUV_Handler),
		(uint8_t *)(uintptr_t)(fd_in2_0910_338x600_FMT_YVU_2P),
		sizeof(fd_in2_0910_338x600_FMT_YVU_2P));
		#endif
	}

	/* Y2R Config */
	YUVConfig_VA = CALL_FROM_CMDQ_FDVT_OPS(fixmap_map,
	FDVT_MetaData->YUVConfig_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "MAP YUVConfig_VA");

	FDVT_ExecMeta->YUVConfig_VA = (uintptr_t)YUVConfig_VA;
	g_FdDrv_Para->FDMODE_YUV2RGB_Config_VA = (uint64_t *)(uintptr_t
	)FDVT_ExecMeta->YUVConfig_VA;

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] YUVConfig_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->YUVConfig_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ",YUVConfig_VA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->YUVConfig_VA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "Y2R Config size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)sizeof(fdvt_FD_yuv2rgb_config));

	if (FDVT_MetaData->FDMode == 0) {
		CALL_FROM_CMDQ_FDVT_OPS(memcpy, (uint8_t *
		)g_FdDrv_Para->FDMODE_YUV2RGB_Config_VA, (uint8_t *)(fdvt_FD_yuv2rgb_config),
		sizeof(fdvt_FD_yuv2rgb_config));
		FDVT_configY2R(FDVT_MetaData, FDVT_ExecMeta);
		CALL_FROM_CMDQ_FDVT_OPS(flush_dcache_to_poc, (void *)(uintptr_t
		)FDVT_ExecMeta->YUVConfig_VA, FDVT_MetaData->YUVConfigSize);
	}

	CALL_FROM_CMDQ_FDVT_OPS(fixmap_unmap);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "UNMAP YUVConfig_VA");

	/* RS Config */
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] RSConfig_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->RSConfig_Handler);

	RSConfig_VA = CALL_FROM_CMDQ_FDVT_OPS(fixmap_map,
	FDVT_MetaData->RSConfig_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "MAP RSConfig_VA");

	FDVT_ExecMeta->RSConfig_VA = (uintptr_t)RSConfig_VA;
	g_FdDrv_Para->FDMODE_RS_Config_VA = (uint64_t *)(uintptr_t
	)FDVT_ExecMeta->RSConfig_VA;

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "RSConfig_VA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->RSConfig_VA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "RS Config size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)sizeof(fdvt_FD_rs_config));

	if (FDVT_MetaData->FDMode == 0) {
		CALL_FROM_CMDQ_FDVT_OPS(memcpy, (uint8_t *)g_FdDrv_Para->FDMODE_RS_Config_VA,
		(uint8_t *)(fdvt_FD_rs_config), sizeof(fdvt_FD_rs_config));
		FDVT_configRS(FDVT_MetaData);
		CALL_FROM_CMDQ_FDVT_OPS(flush_dcache_to_poc, (void *)(uintptr_t
		)FDVT_ExecMeta->RSConfig_VA, FDVT_MetaData->RSConfigSize);
	}

	CALL_FROM_CMDQ_FDVT_OPS(fixmap_unmap);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "UNMAP RSConfig_VA");

	/* FD Config */
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDConfig_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDConfig_Handler);

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FD Config size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)sizeof(fdvt_FD_fd_config));

	if (FDVT_MetaData->FDMode == 0) {
		CALL_FROM_CMDQ_FDVT_OPS(memcpy, &g_FD_Config[0], (uint8_t *)(fdvt_FD_fd_config)
		, sizeof(fdvt_FD_fd_config));
		FDVT_configNetwork(FDVT_MetaData);
		fdvt_memcpy_ops((uint8_t *)(uintptr_t)FDVT_MetaData->FDConfig_Handler,
		(uint8_t *)&g_FD_Config[0], sizeof(fdvt_FD_fd_config));
	}

	/* FD_POSE Config*/
	FD_Pose_Config_VA = CALL_FROM_CMDQ_FDVT_OPS(fixmap_map,
	FDVT_MetaData->FD_POSE_Config_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "MAP FD_Pose_Config_VA");

	FDVT_ExecMeta->FD_Pose_Config_VA = (uintptr_t)FD_Pose_Config_VA;
	g_FdDrv_Para->FDMODE_FD_POSE_Config_VA = (uint64_t *)(uintptr_t
	)FDVT_ExecMeta->FD_Pose_Config_VA;

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FD_Pose_Config_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->FD_Pose_Config_PA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ",FD_Pose_Config_VA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->FD_Pose_Config_VA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FD_Pose_Config_VA size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)sizeof(fdvt_FD_fd_pose_config));

	if (FDVT_MetaData->FDMode == 0) {
		CALL_FROM_CMDQ_FDVT_OPS(memcpy, (uint8_t *
		)g_FdDrv_Para->FDMODE_FD_POSE_Config_VA, (uint8_t *)(fdvt_FD_fd_pose_config),
		sizeof(fdvt_FD_fd_pose_config));
		FDVT_configExtendedNetwork(FDVT_MetaData);
		CALL_FROM_CMDQ_FDVT_OPS(flush_dcache_to_poc, (void *)(uintptr_t
		)FDVT_ExecMeta->FD_Pose_Config_VA, FDVT_MetaData->FD_POSE_ConfigSize);
	}

	CALL_FROM_CMDQ_FDVT_OPS(fixmap_unmap);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "UNMAP FD_Pose_Config_VA");

	/* RS OUT */
	#ifndef OUTPUT_USE_NORMAL_BUFFER
	RSOutBuf_VA = CALL_FROM_CMDQ_FDVT_OPS(fixmap_map,
	FDVT_MetaData->RSOutBuf_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "MAP RSOutBuf_VA");

	FDVT_ExecMeta->RSOutBuf_VA = (uintptr_t)RSOutBuf_VA;

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] RSOutBuf_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->RSOutBuf_PA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG ",RSOutBuf_VA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_ExecMeta->RSOutBuf_VA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "RS OUT size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->RSOutBufSize);

	g_FdDrv_Para->RS_Pyramid0_R_Result_VA = (uint64_t *)(uintptr_t
	)FDVT_ExecMeta->RSOutBuf_VA;
	g_FdDrv_Para->RS_Pyramid0_G_Result_VA = g_FdDrv_Para->RS_Pyramid0_R_Result_VA
	+ fdvt_rs_pyramid0_out_size/8;
	g_FdDrv_Para->RS_Pyramid0_B_Result_VA = g_FdDrv_Para->RS_Pyramid0_G_Result_VA
	+ fdvt_rs_pyramid0_out_size/8;
	g_FdDrv_Para->RS_Pyramid1_R_Result_VA = g_FdDrv_Para->RS_Pyramid0_B_Result_VA
	+ fdvt_rs_pyramid0_out_size/8;
	g_FdDrv_Para->RS_Pyramid1_G_Result_VA = g_FdDrv_Para->RS_Pyramid1_R_Result_VA
	+ fdvt_rs_pyramid1_out_size/8;
	g_FdDrv_Para->RS_Pyramid1_B_Result_VA = g_FdDrv_Para->RS_Pyramid1_G_Result_VA
	+ fdvt_rs_pyramid1_out_size/8;
	g_FdDrv_Para->RS_Pyramid2_R_Result_VA = g_FdDrv_Para->RS_Pyramid1_B_Result_VA
	+ fdvt_rs_pyramid1_out_size/8;
	g_FdDrv_Para->RS_Pyramid2_G_Result_VA = g_FdDrv_Para->RS_Pyramid2_R_Result_VA
	+ fdvt_rs_pyramid2_out_size/8;
	g_FdDrv_Para->RS_Pyramid2_B_Result_VA = g_FdDrv_Para->RS_Pyramid2_G_Result_VA
	+ fdvt_rs_pyramid2_out_size/8;

	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid0_R_Result_VA, 0,
	fdvt_rs_pyramid0_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid0_G_Result_VA, 0,
	fdvt_rs_pyramid0_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid0_B_Result_VA, 0,
	fdvt_rs_pyramid0_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid1_R_Result_VA, 0,
	fdvt_rs_pyramid1_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid1_G_Result_VA, 0,
	fdvt_rs_pyramid1_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid1_B_Result_VA, 0,
	fdvt_rs_pyramid1_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid2_R_Result_VA, 0,
	fdvt_rs_pyramid2_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid2_G_Result_VA, 0,
	fdvt_rs_pyramid2_out_size);
	CALL_FROM_CMDQ_FDVT_OPS(memset, g_FdDrv_Para->RS_Pyramid2_B_Result_VA, 0,
	fdvt_rs_pyramid2_out_size);

	CALL_FROM_CMDQ_FDVT_OPS(flush_dcache_to_poc, (void *)(uintptr_t
	)FDVT_ExecMeta->RSOutBuf_VA, FDVT_MetaData->RSOutBufSize);
	CALL_FROM_CMDQ_FDVT_OPS(fixmap_unmap);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "UNMAP RSOutBuf_VA");
	#endif

	/* FD OUT */
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT] FDOutBuf_PA:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDOutBuf_Handler);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FD OUT size:");
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)FDVT_MetaData->FDOutBufSize);
	FDVT_arrangeOutputAddress_PKVM_PA(FDVT_MetaData);
	FDVT_copyInputDataToBuffer(FDVT_MetaData);

	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "FDVT_configDram OK\n");

	//CMDQ_LOG("Dump YUV Config\n");
	//FDVT_DumpDRAMOut((uint64_t *)g_FdDrv_Para->FDMODE_YUV2RGB_Config_VA, fdvt_fdmode_yuv2rgb_confi_size);
	//CMDQ_LOG("Dump RS Config\n");
	//FDVT_DumpDRAMOut((uint64_t *)g_FdDrv_Para->FDMODE_RS_Config_VA, fdvt_fdmode_rs_confi_size);
	//CMDQ_LOG("Dump FD Config\n");
	//FDVT_DumpDRAMOut((uint64_t *)g_FdDrv_Para->FDMODE_FD_Config_VA, fdvt_fdmode_fd_confi_size);

	return 0;
}

int32_t cmdq_drv_isp_setup_iova(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	struct FDVT_SEC_MetaDataToGCE *FDVT_MetaData;

	FDVT_MetaData = (struct FDVT_SEC_MetaDataToGCE *)(data);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "cmdq drv isp setup iova+");

	if (!data || !isp_execmeta|| !size) {
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT IOVA][err] data:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)data);
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT IOVA][err] isp_execmeta:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)isp_execmeta);
		CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "[FDVT IOVA][err] size:");
		CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)size);
		return 0;
	}
	/* set iova */
	g_RSConfig_IOVA = FDVT_MetaData->RSConfig_IOVA;
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_RSConfig_IOVA);
	g_FDConfig_IOVA = FDVT_MetaData->FDConfig_IOVA;
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_FDConfig_IOVA);
	g_YUVConfig_IOVA = FDVT_MetaData->YUVConfig_IOVA;
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_YUVConfig_IOVA);
	g_FDPOSE_IOVA = FDVT_MetaData->FDPOSE_IOVA;
	CALL_FROM_CMDQ_FDVT_OPS(putx64, (u64)g_FDPOSE_IOVA);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "cmdq drv isp setup iova-");
	return 0;
}


int32_t cmdq_drv_isp_setup_task_cq(
	struct iwc_cq_meta *msgex,
	struct iwc_cq_meta2 *msgex2,
	struct isp_meta_cq *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	return 0;
}
