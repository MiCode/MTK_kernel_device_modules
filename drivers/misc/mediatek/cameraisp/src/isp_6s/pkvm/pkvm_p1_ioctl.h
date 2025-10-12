/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

 #include <linux/ioctl.h>

#define PKVM_P1_DEV_NAME    "pkvm_p1"
#define CAM_PKVM_MAGIC      'P'
#define DAPC_NUM_CQ         34
#define DAPC_NUM_WRITE      15


/*******************************************************************************
 *
 ******************************************************************************/
// ioctl cmd list
enum pkvm_camsys_ioctl_cmd {
	CAM_PKVM_CMD_SUPPORT,
	CAM_PKVM_CMD_GET_HWINFO,
	CAM_PKVM_CMD_SEC_CONFIG,
	CAM_PKVM_CMD_SET_DAPC_REG,
	CAM_PKVM_CMD_SET_SEC_CAM,
	CAM_PKVM_CMD_GET_FH_INFO,
	CAM_PKVM_CMD_UNINIT,
	CAM_PKVM_CMD_CLOSE
};

// MEM Type
enum pkvm_mem_type {
	_SECMEM_CQ_DESCRIPTOR_TABLE = 0,
	_SECMEM_VIRTUAL_REG_TABLE,
	_SECMEM_FRAME_HEADER,
	_SECMEM_LSC,
	_SECMEM_BPC,
	_SECMEM_TWIN_FRAME_HEADER,
	_SECMEM_MAX
};

// SecMgr_RegInfo_PKVM: For DAPC registers setting
struct SecMgr_RegInfo_PKVM{
	uint32_t  dapc_cq[DAPC_NUM_CQ];
	uint32_t  CamModule;
};

// SecMgr_SecInfo_PKVM: for FH
struct SecMgr_SecInfo_PKVM{
	uint32_t     port;
	uint64_t     sec_pa;
	uint32_t     sec_fhinfo[15];
};

// SecMgr_QueryInfo_PKVM: for query HWInfo
struct SecMgr_QueryInfo_PKVM{
	uint8_t  Num_of_Cam;
	uint32_t CAM_CTL_DMA_EN;
	uint32_t SecReg_ADDR_CQ[DAPC_NUM_CQ];
};


/*******************************************************************************
 *
 ******************************************************************************/
#define CAM_PKVM_SUPPORT        \
	_IOR(CAM_PKVM_MAGIC, CAM_PKVM_CMD_SUPPORT, bool)

#define CAM_PKVM_GET_HWINFO     \
	_IOWR(CAM_PKVM_MAGIC, CAM_PKVM_CMD_GET_HWINFO, struct SecMgr_QueryInfo_PKVM)

#define CAM_PKVM_SEC_CONFIG     \
	_IOW(CAM_PKVM_MAGIC, CAM_PKVM_CMD_SEC_CONFIG, uint32_t)

#define CAM_PKVM_SET_DAPC_REG   \
	_IOWR(CAM_PKVM_MAGIC, CAM_PKVM_CMD_SET_DAPC_REG, struct SecMgr_RegInfo_PKVM)

#define CAM_PKVM_SET_SEC_CAM    \
	_IOWR(CAM_PKVM_MAGIC, CAM_PKVM_CMD_SET_SEC_CAM, uint32_t)

#define CAM_PKVM_GET_FH_INFO    \
	_IOWR(CAM_PKVM_MAGIC, CAM_PKVM_CMD_GET_FH_INFO, struct SecMgr_SecInfo_PKVM)

#define CAM_PKVM_UNINIT          \
	_IOW(CAM_PKVM_MAGIC, CAM_PKVM_CMD_UNINIT, uint32_t)

#define CAM_PKVM_CLOSE          \
	_IOW(CAM_PKVM_MAGIC, CAM_PKVM_CMD_CLOSE, uint32_t)
