/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <pkvm_sys.h>


/*******************************************************************************
 * Defines
 ******************************************************************************/
#define _DAPC_NUM_CQ         34
#define _DAPC_NUM_WRITE      15
#define TG_A	0
#define TG_B	1
#define CAM_NUM 2

#define FH_IDX_TS_LSB        (0)
#define FH_IDX_MAGIC         (1)
#define FH_IDX_ENQUE_SOF     (2)
#define FH_IDX_RAW_TYPE      (3)
#define FH_IDX_PIX_ID        (4)
#define FH_IDX_FMT           (5)
#define FH_IDX_ENQUE_CNT     (6)
#define FH_IDX_SRC_SIZE      (7)
#define FH_IDX_IQ_LEVEL      (8)
#define FH_IDX_CROP_START    (9)
#define FH_IDX_CROP_SIZE    (10)
#define FH_IDX_IMG_PA       (11)
#define FH_IDX_TS_MSB       (12)
#define FH_IDX_IS_UFE_FMT   (13)
#define FH_IDX_MAX          (14)

#define RET_OK          (0x1)
#define RET_BYPASS      (0x2)

#define MTRUE           (0x1)
#define MFALSE          (0x0)

/*******************************************************************************
 * Constants
 ******************************************************************************/
static const uint16_t gisp_drv_reg_addr[_DAPC_NUM_CQ] = {
	0x000,
	0x004,
	0x008,
	0x00C,
	0x010,
	0x014,
	0x018,
	0x040,
	0x044,
	0x930,
	0x934,
	0x938,
	0x93C,
	0xA08,
	0xC48,
	0x808,
	0x1648,
	0x1398,
	0x5040,
	0x5044,
	0x5048,
	0x504C,
	0x5050,
	0x5054,
	0x5058,
	0x505C,
	0x5060,
	0x5064,
	0x5068,
	0x506C,
	0x5070,
	0x5074,
	0x5078,
	0x507C
};


/*******************************************************************************
 * Enums & Variables
 ******************************************************************************/
enum pkvm_sec_dma_port {
	CAM_IMGO_R1_A = 0,
	CAM_RRZO_R1_A,
	CAM_CQI_R1_A,
	CAM_BPCI_R1_A,
	CAM_YUVO_R1_A,
	CAM_UFDI_R2_A,
	CAM_RAWI_R2_A,
	CAM_RAWI_R3_A,
	CAM_AAO_R1_A,
	CAM_AFO_R1_A,
	CAM_FLKO_R1_A,
	CAM_LCESO_R1_A,
	CAM_CRZO_R1_A,
	CAM_LTMSO_R1_A,
	CAM_RSSO_R1_A,
	CAM_AAHO_R1_A,
	CAM_LSCI_R1_A,
	/* larb17 -- 17*/
	CAM_IMGO_R1_B,
	CAM_RRZO_R1_B,
	CAM_CQI_R1_B,
	CAM_BPCI_R1_B,
	CAM_YUVO_R1_B,
	CAM_UFDI_R2_B,
	CAM_RAWI_R2_B,
	CAM_RAWI_R3_B,
	CAM_AAO_R1_B,
	CAM_AFO_R1_B,
	CAM_FLKO_R1_B,
	CAM_LCESO_R1_B,
	CAM_CRZO_R1_B,
	CAM_LTMSO_R1_B,
	CAM_RSSO_R1_B,
	CAM_AAHO_R1_B,
	CAM_LSCI_R1_B,
	CAM_DMA_PORT_MAX
};

uint32_t port_sec_enable[CAM_DMA_PORT_MAX] = {0};
