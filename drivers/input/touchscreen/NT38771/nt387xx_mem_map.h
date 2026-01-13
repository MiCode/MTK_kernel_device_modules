/*
 * Copyright (C) 2024 Novatek, Inc.
 *
 * $Revision: 67976 $
 * $Date: 2020-08-27 16:49:50 +0800 (週四, 27 八月 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


typedef struct nvt_ts_reg {
	uint32_t addr; /* byte in which address */
	uint8_t mask; /* in which bits of that byte */
} nvt_ts_reg_t;

struct nvt_ts_mem_map {
	uint32_t EVENT_BUF_ADDR;
	uint32_t RAW_PIPE0_ADDR;
	uint32_t RAW_PIPE1_ADDR;
	uint32_t BASELINE_ADDR;
	uint32_t DIFF_PIPE0_ADDR;
	uint32_t DIFF_PIPE1_ADDR;
	uint32_t TX_SELF_RAWIIR_ADDR;
	uint32_t RX_SELF_RAWIIR_ADDR;
  /*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	uint32_t TX_SELF_DIFF_ADDR;
	uint32_t RX_SELF_DIFF_ADDR;
	uint32_t RAWDATA_FLATNESS_INFO_ADDR;
  /*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	uint32_t PEN_2D_BL_TIP_X_ADDR;
	uint32_t PEN_2D_BL_TIP_Y_ADDR;
	uint32_t PEN_2D_BL_RING_X_ADDR;
	uint32_t PEN_2D_BL_RING_Y_ADDR;
	uint32_t PEN_2D_DIFF_TIP_X_ADDR;
	uint32_t PEN_2D_DIFF_TIP_Y_ADDR;
	uint32_t PEN_2D_DIFF_RING_X_ADDR;
	uint32_t PEN_2D_DIFF_RING_Y_ADDR;
	uint32_t PEN_2D_RAW_TIP_X_ADDR;
	uint32_t PEN_2D_RAW_TIP_Y_ADDR;
	uint32_t PEN_2D_RAW_RING_X_ADDR;
	uint32_t PEN_2D_RAW_RING_Y_ADDR;
	nvt_ts_reg_t ENB_CASC_REG;
	/* FW History */
	uint32_t MMAP_HISTORY_EVENT0;
	uint32_t MMAP_HISTORY_EVENT1;
	uint32_t MMAP_HISTORY_EVENT0_ICS;
	uint32_t MMAP_HISTORY_EVENT1_ICS;
	/* Phase 2 Host Download */
	uint32_t BOOT_RDY_ADDR;
	uint32_t ACI_ERR_CLR_ADDR;
	uint32_t POR_CD_ADDR;
	uint32_t TX_AUTO_COPY_EN;
	uint32_t SPI_DMA_TX_INFO;
	/* BLD CRC */
	uint32_t BLD_LENGTH_ADDR;
	uint32_t ILM_LENGTH_ADDR;
	uint32_t DLM_LENGTH_ADDR;
	uint32_t BLD_DES_ADDR;
	uint32_t ILM_DES_ADDR;
	uint32_t DLM_DES_ADDR;
	uint32_t G_DMA_CHECKSUM_ADDR;
	uint32_t G_ILM_CHECKSUM_ADDR;
	uint32_t G_DLM_CHECKSUM_ADDR;
	uint32_t R_DMA_CHECKSUM_ADDR;
	uint32_t R_ILM_CHECKSUM_ADDR;
	uint32_t R_DLM_CHECKSUM_ADDR;
	uint32_t DMA_CRC_EN_ADDR;
	uint32_t BLD_ILM_DLM_CRC_ADDR;
	uint32_t DMA_CRC_FLAG_ADDR;
	uint32_t PH2_FLASH_EXIST_CODE_ADDR;
	nvt_ts_reg_t NORFW_HEADER_CRC_DONE_ADDR;
	nvt_ts_reg_t NORFW_FINAL_HEADER_DONE_ADDR;
};

struct nvt_ts_hw_info {
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint8_t bld_multi_header;
	bool use_gcm;
	const struct nvt_ts_hw_reg_addr_info *hw_regs;
};

typedef enum {
	HWCRC_NOSUPPORT  = 0x00,
	HWCRC_LEN_2Bytes = 0x01,
	HWCRC_LEN_3Bytes = 0x02,
} HWCRCBankConfig;

typedef enum {
	AUTOCOPY_NOSUPPORT    = 0x00,
	CHECK_SPI_DMA_TX_INFO = 0x01,
	CHECK_TX_AUTO_COPY_EN = 0x02,
} AUTOCOPYCheck;

typedef enum {
	BLDMHEADER_NOSUPPORT      = 0x00,
	BLDMHEADER_SUPPORT        = 0x01,
	BLDMHEADER_PH2FLASH_ONE   = 0x02,
	BLDMHEADER_PH2FLASH_MULTI = 0x03,
} BLDMultiHeader;

/* hw info reg*/
struct nvt_ts_hw_reg_addr_info {
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t bld_spe_pups_addr;
};

static const struct nvt_ts_hw_reg_addr_info hw_reg_addr_info_old_spe2 = {
	.chip_ver_trim_addr = 0x3F004,
	.swrst_sif_addr = 0x3F0FE,
	.bld_spe_pups_addr = 0x3F12A,
};

/* tddi */
static const struct nvt_ts_mem_map NT38771_memory_map = {
	.EVENT_BUF_ADDR               = 0x2B400,
	.RAW_PIPE0_ADDR               = 0x2EEC8,
	.RAW_PIPE1_ADDR               = 0x2EEC8,
	.BASELINE_ADDR                = 0x304A8,
	.DIFF_PIPE0_ADDR              = 0x2F828,
	.DIFF_PIPE1_ADDR              = 0x2FE68,
	.TX_SELF_RAWIIR_ADDR          = 0x313A8,
	.RX_SELF_RAWIIR_ADDR          = 0x312B8,
  /*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 start*/
	.TX_SELF_DIFF_ADDR            = 0x31380,
	.RX_SELF_DIFF_ADDR            = 0x31268,
	.RAWDATA_FLATNESS_INFO_ADDR   = 0x2B4B0,
  /*P16 code for HQFEAT-94200 by xiongdejun at 2025/4/15 end*/
	.ENB_CASC_REG                 = {.addr = 0, .mask = 0},
	/* FW History */
	.MMAP_HISTORY_EVENT0          = 0x2B5C0,
	.MMAP_HISTORY_EVENT1          = 0x2B600,
	/* Phase 2 Host Download */
	.BOOT_RDY_ADDR                = 0x3F102,
	.ACI_ERR_CLR_ADDR             = 0x3F705,
	/* BLD CRC */
	.BLD_LENGTH_ADDR              = 0x3F10C,	//0x3F10C ~ 0x3F10E	(3 bytes)
	.ILM_LENGTH_ADDR              = 0x3F7B8,	//0x3F7B8 ~ 0x3F7BA	(3 bytes)
	.DLM_LENGTH_ADDR              = 0x3FB88,	//0x3FB88 ~ 0x3FB8A	(3 bytes)
	.BLD_DES_ADDR                 = 0x3F108,	//0x3F108 ~ 0x3F10E	(3 bytes)
	.ILM_DES_ADDR                 = 0x3F7B4,	//0x3F7B4 ~ 0x3F7B6	(3 bytes)
	.DLM_DES_ADDR                 = 0x3FB84,	//0x3FB84 ~ 0x3FB86	(3 bytes)
	.G_DMA_CHECKSUM_ADDR          = 0x3F110,	//0x3F110 ~ 0x3F113	(4 bytes)
	.G_ILM_CHECKSUM_ADDR          = 0x3F7BC,	//0x3F7BC ~ 0x3F7BF	(4 bytes)
	.G_DLM_CHECKSUM_ADDR          = 0x3FB8C,	//0x3FB8C ~ 0x3FB8F	(4 bytes)
	.R_DMA_CHECKSUM_ADDR          = 0x3F114,	//0x3F114 ~ 0x3F117	(4 bytes)
	.R_ILM_CHECKSUM_ADDR          = 0x3FBBC,	//0x3FBBC ~ 0x3FBBF (4 bytes)
	.R_DLM_CHECKSUM_ADDR          = 0x3FBC0,	//0x3FBC0 ~ 0x3FBC3 (4 bytes)
	.DMA_CRC_EN_ADDR              = 0x3F10F,
	.BLD_ILM_DLM_CRC_ADDR         = 0x3F127,
	.DMA_CRC_FLAG_ADDR            = 0x3F129,
	.PH2_FLASH_EXIST_CODE_ADDR    = 0x3FACB,
	.NORFW_HEADER_CRC_DONE_ADDR   = {.addr = 0x3F128, .mask = 1},
	.NORFW_FINAL_HEADER_DONE_ADDR = {.addr = 0x3F128, .mask = 2},
};

static struct nvt_ts_hw_info NT38771_hw_info = {
	.hw_crc    = HWCRC_LEN_3Bytes,
	.auto_copy = CHECK_SPI_DMA_TX_INFO,
	.bld_multi_header = BLDMHEADER_PH2FLASH_ONE,
	.use_gcm   = 1,
	.hw_regs   = &hw_reg_addr_info_old_spe2,
};

#define NVT_ID_BYTE_MAX 6
struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_mem_map *mmap_casc;
	const struct nvt_ts_hw_info *hwinfo;
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x87, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT38771_memory_map, .hwinfo = &NT38771_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x71, 0x87, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT38771_memory_map, .hwinfo = &NT38771_hw_info},
};
