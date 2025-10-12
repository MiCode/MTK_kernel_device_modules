/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __ISP_SEC_PLATFORM_H__
#define __ISP_SEC_PLATFORM_H__

typedef unsigned int FIELD;
typedef unsigned int UINT32;

#ifdef BIT
#undef BIT
#define BIT(nr)	(1UL << (nr))
#endif
#define INNER_BASE      0x8000
#define CAMA_BASE       0x3a700000
#define CAMB_BASE       0x3a900000
#define CAMC_BASE       0x3ab00000
#define CAMA_RMSBASE    0x3a740000
#define CAMB_RMSBASE    0x3a940000
#define CAMC_RMSBASE    0x3aa40000
#define CAMA_YUVBASE    0x3a780000
#define CAMB_YUVBASE    0x3a980000
#define CAMC_YUVBASE    0x3aa80000
#define RAWA_ROOT_BASE  0x3a7e0000
#define RAWB_ROOT_BASE  0x3a9e0000
#define RAWC_ROOT_BASE  0x3abe0000
#define RMSA_ROOT_BASE  0x3a800000
#define RMSB_ROOT_BASE  0x3aa00000
#define RMSC_ROOT_BASE  0x3ac00000
#define YUVA_ROOT_BASE  0x3a820000
#define YUVB_ROOT_BASE  0x3aa20000
#define YUVC_ROOT_BASE  0x3ac20000
#define CAMSVCENTRAL_CID_CHK_SENINF    0x01D4
#define CAMSVCENTRAL_CID_CHK_RAW_DCIF    0x01D8
#define CAMSVA_CENTRAL_ROOT_BASE 0x3a699000
#define CAMSVB_CENTRAL_ROOT_BASE 0x3a69a000
#define CAMSVC_CENTRAL_ROOT_BASE 0x3a69b000
#define CAMSVCENTRAL_ROOT_CAMSV_CID 0x4
#define CAMSV_DMA_SHIFT 0x100
#define REG_CAMSV_DMATOP_DMA_DEBUG_SEL			0x0090
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT			0x0094
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT2		0x009C
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT3		0x00A0
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT4		0x00A4
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT5		0x00A8
#define REG_CAMSV_ERR_CCU_STATUS_EN		0x034C
#define REG_CAMSV_ERR_CCU_STATUS		0x0350
#define REG_FHG_FHG_SPARE_3                        0xFEC
#define REG_FRAME_IDX                              REG_FHG_FHG_SPARE_3
#define CAMSV_DMA_BASE 0x3A600000
#define CAMSV_ENG_NUM 6
#define CAMSV_TAG_IMG_START 0
#define CAMSV_TAG_IMG_END   8
#define CAMSV_ENG_OFFSET 0x10000
#define QOF_CAM_TOP 0x3a0a0000
#define QOF_CAM_TOP_QOF_TOP_CTL 0x0
#define QOF_CAM_TOP_ON_LOCK_1   BIT(21)
#define QOF_CAM_TOP_OFF_LOCK_1   BIT(22)
#define QOF_CAM_TOP_OUT_LOCK_1   BIT(23)
#define QOF_CAM_TOP_ON_LOCK_2   BIT(24)
#define QOF_CAM_TOP_OFF_LOCK_2   BIT(25)
#define QOF_CAM_TOP_OUT_LOCK_2   BIT(26)
#define QOF_CAM_TOP_ON_LOCK_3   BIT(27)
#define QOF_CAM_TOP_OFF_LOCK_3   BIT(28)
#define QOF_CAM_TOP_OUT_LOCK_3   BIT(29)
/*Offset*/
// RAWA_ROOT_BASE: 0x3a7e2000 | RAWB_ROOT_BASE  0x3a9e2000 | RAWC_ROOT_BASE  0x3abe2000
#define CAMCTL_ROOT_CID                     0x0     //0x3a7e2000
#define CAMCTL_ROOT_STAT_PROT_EN            0x10    //0x3a7e010
#define CAMCTL_ROOT_STAT_PROT_SIZE0         0x14    //0x3a7e2014
#define CAMCTL_ROOT_STAT_PROT_SIZE1         0x18    //0x3a7e2018
#define CAMCTL_ROOT_STAT_PROT_SIZE2         0x1C    //0x3a7e201c
#define CAMCTL_ROOT_STAT_PROT_SIZE3         0x20    //0x3a7e2020
#define CAMCTL_ROOT_STAT_PROT_SIZE4         0x24    //0x3a7e2024
#define CAMCTL_ROOT_STAT_PROT_SIZE5         0x28    //0x3a7e2028
#define CAMCTL_ROOT_STAT_PROT_SIZE6         0x2C    //0x3a7e202c
#define CAMCTL_ROOT_STAT_PROT_SIZE7         0x30    //0x3a7e2030
#define CAMCTL_ROOT_RAW_SEL_SECURE_LOCK     0x38    //0x3a7e2038
// RMSA_ROOT_BASE  0x3a801000
#define CAMCTL3_ROOT_CID                    0x0//0x1000
#define CAMCTL3_ROOT_STAT_PROT_EN           0x10//0x1010
#define CAMCTL3_ROOT_STAT_PROT_SIZE0        0x14//0x1014
#define CAMCTL3_ROOT_STAT_PROT_SIZE1        0x18//0x1018
// YUVA_ROOT_BASE  0x3a822000
#define CAMCTL2_ROOT_CID                    0x0//0x2000
#define CAMCTL2_ROOT_STAT_PROT_EN           0x10//0x2010
#define CAMCTL2_ROOT_STAT_PROT_SIZE0        0x14//0x2014
// RAWA_ROOT_BASE: 0x3a7e2000
#define CAMRAWDMA_ROOT_SECURE_CTRL          0x1000  //0x3a7e3000
#define CAMRAWDMA_ROOT_DOMAIN_REGISTER_2    0x100C  //0x3a7e300c
#define CAMRAWDMA_ROOT_DOMAIN_REGISTER_3    0x1010  //0x3a7e3010
#define CAMRAWDMA_ROOT_DOMAIN_REGISTER_6    0x101C  //0x3a7e301c
#define CAMRAWDMA_ROOT_DOMAIN_REGISTER_9    0x1028  //0x3a7e3028
#define CAMRAWDMA_ROOT_SECURE_REGISTER_0    0x1048  //0x3a7e3048
#define CAMRAWDMA_ROOT_SECURE_REGISTER_1    0x1060  //0x3a7e3060
// YUVA_ROOT_BASE  0x3a822000
#define CAMYUVDMA_ROOT_SECURE_CTRL          0x1000  //0x3a823000
#define CAMYUVDMA_ROOT_DOMAIN_REGISTER_0    0x1004  //0x3a823004
#define CAMYUVDMA_ROOT_DOMAIN_REGISTER_1    0x1008  //0x3a823008
#define CAMYUVDMA_ROOT_DOMAIN_REGISTER_2    0x100C  //0x3a82300c
#define CAMYUVDMA_ROOT_DOMAIN_REGISTER_3    0x1010  //0x3a823010
#define CAMYUVDMA_ROOT_DOMAIN_REGISTER_4    0x1014  //0x3a823014
#define CAMYUVDMA_ROOT_SECURE_REGISTER_0    0x1018  //0x3a823018
/*Offset end*/
/*Default value*/
#define CAMCTL_TIF_DL_EN                    0x1c0
#define CAMCTL_OTR_DL_EN                    0x1c4
#define CAMCTL3_TIF_DL_EN                   0x1c0
#define CAMCTL2_TIF_DL_EN                   0x1c0
#define CAMCTL_ROOT_CID_VALUE               0xE0E
#define CAMCTL3_ROOT_CID_VALUE              0xE
#define CAMCTL2_ROOT_CID_VALUE              0xE
#define CAMSV_ROOT_CID_VALUE                0xE
#define CAMCTL_ROOT_PROT_VALUE              0x7f
#define CAMCTL3_ROOT_PROT_VALUE             0x7
#define CAMCTL2_ROOT_PROT_VALUE             0x1
#define RAW_ROOT_SECURE_CTRL_VALUE          0x9
#define YUV_ROOT_SECURE_CTRL_VALUE          0x9
#define RAW_SECURE_VALUE_0                  0x3f00
#define RAW_SECURE_VALUE_1                  0xf003
#define YUV_SECURE_VALUE_0                  0xfcfff
#define RAW_DOMAIN_VALUE_2                  0x97979797
#define RAW_DOMAIN_VALUE_3                  0x9797
#define RAW_DOMAIN_VALUE_6                  0x979700
#define RAW_DOMAIN_VALUE_9                  0x97979797
#define YUV_DOMAIN_VALUE_0                  0x97979797
#define YUV_DOMAIN_VALUE_1                  0x97979797
#define YUV_DOMAIN_VALUE_2                  0x97979797
#define YUV_DOMAIN_VALUE_3                  0x97979797
#define YUV_DOMAIN_VALUE_4                  0x97970000
#define CAMCTL_TIF_DL_EN_VALUE              0x7fff
#define CAMCTL_OTR_DL_EN_VALUE              0xfc
#define CAMCTL3_TIF_DL_EN_VALUE             0x3
#define CAMCTL2_TIF_DL_EN_VALUE             0x3f
#define CAMSV_DMA_VALUE                     0x80000097

/*Default value end*/
typedef union {
		struct /* 0x3A600214 */ {
				FIELD CAMSVCENTRAL_CAMSV_SENINF_CID_CHK_EN                        :  1;    /*  0.. 0, 0x00000001 */
				FIELD rsv_1                                                       :  3;    /*  1.. 3, 0x0000000e */
				FIELD CAMSVCENTRAL_CAMSV_SENINF_VIO_CID                           :  5;    /*  4.. 8, 0x000001f0 */
				FIELD rsv_9                                                       : 23;    /*  9..31, 0xfffffe00 */
		} Bits;
		UINT32 Raw;
} REG_E_CAMSVCENTRAL_CID_CHK_SENINF;
typedef union {
		struct /* 0x3A600218 */ {
				FIELD CAMSVCENTRAL_RAW_TO_CAMSV_DCIF_CID_CHK_EN                   :  1;    /*  0.. 0, 0x00000001 */
				FIELD rsv_1                                                       :  3;    /*  1.. 3, 0x0000000e */
				FIELD CAMSVCENTRAL_RAW_TO_CAMSV_DCIF_VIO_CID                      :  5;    /*  4.. 8, 0x000001f0 */
				FIELD rsv_9                                                       : 23;    /*  9..31, 0xfffffe00 */
		} Bits;
		UINT32 Raw;
} REG_E_CAMSVCENTRAL_CID_CHK_RAW_DCIF;
typedef union {
		struct /* 0x3A7E2000 */ {
				FIELD CAMCTL_ROOT_p1_cid                                          :  5;    /*  0.. 4, 0x0000001f */
				FIELD rsv_5                                                       :  3;    /*  5.. 7, 0x000000e0 */
				FIELD CAMCTL_ROOT_cq_cid                                          :  5;    /*  8..12, 0x00001f00 */
				FIELD rsv_13                                                      : 19;    /* 13..31, 0xffffe000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_CID;
typedef union {
		struct /* 0x3A7E2010 */ {
				FIELD CAMCTL_ROOT_aestat_r1_stat_prot_en                          :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMCTL_ROOT_awb_r1_stat_prot_en                             :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMCTL_ROOT_af_r1_stat_prot_en                              :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMCTL_ROOT_tsfs_r1_stat_prot_en                            :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMCTL_ROOT_tsfs_r2_stat_prot_en                            :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMCTL_ROOT_flk_r1_stat_prot_en                             :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMCTL_ROOT_ltms_r1_stat_prot_en                            :  1;    /*  6.. 6, 0x00000040 */
				FIELD rsv_7                                                       : 25;    /*  7..31, 0xffffff80 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_EN;
typedef union {
		struct /* 0x3A7E2014 */ {
				FIELD CAMCTL_ROOT_aestat_pix_x                                    : 13;    /*  0..12, 0x00001fff */
				FIELD rsv_13                                                      :  3;    /* 13..15, 0x0000e000 */
				FIELD CAMCTL_ROOT_aestat_pix_y                                    : 13;    /* 16..28, 0x1fff0000 */
				FIELD rsv_29                                                      :  3;    /* 29..31, 0xe0000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE0;
typedef union {
		struct /* 0x3A7E2018 */ {
				FIELD CAMCTL_ROOT_aestat_w_hsize                                  : 14;    /*  0..13, 0x00003fff */
				FIELD rsv_14                                                      :  2;    /* 14..15, 0x0000c000 */
				FIELD CAMCTL_ROOT_aestat_w_vsize                                  : 14;    /* 16..29, 0x3fff0000 */
				FIELD rsv_30                                                      :  2;    /* 30..31, 0xc0000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE1;
typedef union {
		struct /* 0x3A7E201C */ {
				FIELD CAMCTL_ROOT_awb_w_hsize                                     : 14;    /*  0..13, 0x00003fff */
				FIELD rsv_14                                                      :  2;    /* 14..15, 0x0000c000 */
				FIELD CAMCTL_ROOT_awb_w_vsize                                     : 14;    /* 16..29, 0x3fff0000 */
				FIELD rsv_30                                                      :  2;    /* 30..31, 0xc0000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE2;
typedef union {
		struct /* 0x3A7E2020 */ {
				FIELD CAMCTL_ROOT_af_blk_xsize                                    :  6;    /*  0.. 5, 0x0000003f */
				FIELD rsv_6                                                       : 10;    /*  6..15, 0x0000ffc0 */
				FIELD CAMCTL_ROOT_af_blk_ysize                                    :  7;    /* 16..22, 0x007f0000 */
				FIELD rsv_23                                                      :  9;    /* 23..31, 0xff800000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE3;
typedef union {
		struct /* 0x3A7E2024 */ {
				FIELD CAMCTL_ROOT_tsfs_w_hsize                                    : 10;    /*  0.. 9, 0x000003ff */
				FIELD rsv_10                                                      :  6;    /* 10..15, 0x0000fc00 */
				FIELD CAMCTL_ROOT_tsfs_w_vsize                                    : 10;    /* 16..25, 0x03ff0000 */
				FIELD rsv_26                                                      :  6;    /* 26..31, 0xfc000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE4;
typedef union {
		struct /* 0x3A7E2028 */ {
				FIELD CAMCTL_ROOT_flk_size_x                                      : 16;    /*  0..15, 0x0000ffff */
				FIELD CAMCTL_ROOT_flk_size_y                                      : 16;    /* 16..31, 0xffff0000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE5;
typedef union {
		struct /* 0x3A7E202C */ {
				FIELD CAMCTL_ROOT_ltms_blk_width                                  : 16;    /*  0..15, 0x0000ffff */
				FIELD CAMCTL_ROOT_ltms_blk_height                                 : 16;    /* 16..31, 0xffff0000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE6;
typedef union {
		struct /* 0x3A7E2030 */ {
				FIELD CAMCTL_ROOT_ltms_glbhist_win_x                              : 16;    /*  0..15, 0x0000ffff */
				FIELD CAMCTL_ROOT_ltms_glbhist_win_y                              : 16;    /* 16..31, 0xffff0000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_STAT_PROT_SIZE7;
typedef union {
		struct /* 0x3A7E2038 */ {
				FIELD CAMCTL_ROOT_raw_sel_secure_lock_en                          :  1;    /*  0.. 0, 0x00000001 */
				FIELD rsv_1                                                       :  3;    /*  1.. 3, 0x0000000e */
				FIELD CAMCTL_ROOT_raw_sel_secure_lock_value                       :  3;    /*  4.. 6, 0x00000070 */
				FIELD rsv_7                                                       : 25;    /*  7..31, 0xffffff80 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_ROOT_RAW_SEL_SECURE_LOCK;
typedef union {
		struct /* 0x3A7E3000 */ {
				FIELD CAMRAWDMA_ROOT_LSID                                         :  4;    /*  0.. 3, 0x0000000f */
				FIELD rsv_4                                                       : 12;    /*  4..15, 0x0000fff0 */
				FIELD CAMRAWDMA_ROOT_UID_SEL                                      :  1;    /* 16..16, 0x00010000 */
				FIELD rsv_17                                                      : 15;    /* 17..31, 0xfffe0000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_SECURE_CTRL;
typedef union {
		struct /* 0x3A7E300C */ {
				FIELD CAMRAWDMA_ROOT_RAWI_R2_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMRAWDMA_ROOT_UFDI_R2_GDOMAIN                              :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMRAWDMA_ROOT_RAWI_R3_GDOMAIN                              :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMRAWDMA_ROOT_UFDI_R3_GDOMAIN                              :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_2;
typedef union {
		struct /* 0x3A7E3010 */ {
				FIELD CAMRAWDMA_ROOT_RAWI_R5_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMRAWDMA_ROOT_UFDI_R5_GDOMAIN                              :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMRAWDMA_ROOT_BPCI_R1_GDOMAIN                              :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMRAWDMA_ROOT_BPCI_R2_GDOMAIN                              :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_3;
typedef union {
		struct /* 0x3A7E301C */ {
				FIELD CAMRAWDMA_ROOT_CACI_R1_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMRAWDMA_ROOT_IMGO_R1_GDOMAIN                              :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMRAWDMA_ROOT_UFEO_R1_GDOMAIN                              :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMRAWDMA_ROOT_FHO_R1_GDOMAIN                               :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_6;
typedef union {
		struct /* 0x3A7E3028 */ {
				FIELD CAMRAWDMA_ROOT_DRZB2NO_R1_GDOMAIN                           :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMRAWDMA_ROOT_DRZB2NBO_R1_GDOMAIN                          :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMRAWDMA_ROOT_DRZB2NCO_R1_GDOMAIN                          :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMRAWDMA_ROOT_DRZB2NDO_R1_GDOMAIN                          :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_DOMAIN_REGISTER_9;
typedef union {
		struct /* 0x3A7E3048 */ {
				FIELD CAMRAWDMA_ROOT_CQI_R1_GSECURE                               :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMRAWDMA_ROOT_CQI_R2_GSECURE                               :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMRAWDMA_ROOT_CQI_R3_GSECURE                               :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMRAWDMA_ROOT_CQI_R4_GSECURE                               :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMRAWDMA_ROOT_CQI_R5_GSECURE                               :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMRAWDMA_ROOT_CQI_R6_GSECURE                               :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMRAWDMA_ROOT_CQI_R7_GSECURE                               :  1;    /*  6.. 6, 0x00000040 */
				FIELD CAMRAWDMA_ROOT_CQI_R8_GSECURE                               :  1;    /*  7.. 7, 0x00000080 */
				FIELD CAMRAWDMA_ROOT_RAWI_R2_GSECURE                              :  1;    /*  8.. 8, 0x00000100 */
				FIELD CAMRAWDMA_ROOT_UFDI_R2_GSECURE                              :  1;    /*  9.. 9, 0x00000200 */
				FIELD CAMRAWDMA_ROOT_RAWI_R3_GSECURE                              :  1;    /* 10..10, 0x00000400 */
				FIELD CAMRAWDMA_ROOT_UFDI_R3_GSECURE                              :  1;    /* 11..11, 0x00000800 */
				FIELD CAMRAWDMA_ROOT_RAWI_R5_GSECURE                              :  1;    /* 12..12, 0x00001000 */
				FIELD CAMRAWDMA_ROOT_UFDI_R5_GSECURE                              :  1;    /* 13..13, 0x00002000 */
				FIELD CAMRAWDMA_ROOT_BPCI_R1_GSECURE                              :  1;    /* 14..14, 0x00004000 */
				FIELD CAMRAWDMA_ROOT_BPCI_R2_GSECURE                              :  1;    /* 15..15, 0x00008000 */
				FIELD CAMRAWDMA_ROOT_BPCI_R3_GSECURE                              :  1;    /* 16..16, 0x00010000 */
				FIELD CAMRAWDMA_ROOT_FPRI_R1_GSECURE                              :  1;    /* 17..17, 0x00020000 */
				FIELD CAMRAWDMA_ROOT_LSCI_R1_GSECURE                              :  1;    /* 18..18, 0x00040000 */
				FIELD CAMRAWDMA_ROOT_LSCI_R2_GSECURE                              :  1;    /* 19..19, 0x00080000 */
				FIELD CAMRAWDMA_ROOT_PDI_R1_GSECURE                               :  1;    /* 20..20, 0x00100000 */
				FIELD CAMRAWDMA_ROOT_AEI_R1_GSECURE                               :  1;    /* 21..21, 0x00200000 */
				FIELD CAMRAWDMA_ROOT_LTMSCTI_R1_GSECURE                           :  1;    /* 22..22, 0x00400000 */
				FIELD CAMRAWDMA_ROOT_GRMGI_R1_GSECURE                             :  1;    /* 23..23, 0x00800000 */
				FIELD CAMRAWDMA_ROOT_CACI_R1_GSECURE                              :  1;    /* 24..24, 0x01000000 */
				FIELD CAMRAWDMA_ROOT_MLSCI_R1_GSECURE                             :  1;    /* 25..25, 0x02000000 */
				FIELD CAMRAWDMA_ROOT_LTMSTI_R1_GSECURE                            :  1;    /* 26..26, 0x04000000 */
				FIELD CAMRAWDMA_ROOT_LTMSTI_R2_GSECURE                            :  1;    /* 27..27, 0x08000000 */
				FIELD CAMRAWDMA_ROOT_TCYSI_R1_GSECURE                             :  1;    /* 28..28, 0x10000000 */
				FIELD CAMRAWDMA_ROOT_AEHI_R1_GSECURE                              :  1;    /* 29..29, 0x20000000 */
				FIELD CAMRAWDMA_ROOT_AEDI_R1_GSECURE                              :  1;    /* 30..30, 0x40000000 */
				FIELD rsv_31                                                      :  1;    /* 31..31, 0x80000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_SECURE_REGISTER_0;
typedef union {
		struct /* 0x3A7E3060 */ {
				FIELD CAMRAWDMA_ROOT_IMGO_R1_GSECURE                              :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMRAWDMA_ROOT_UFEO_R1_GSECURE                              :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMRAWDMA_ROOT_FLKO_R1_GSECURE                              :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMRAWDMA_ROOT_FHO_R1_GSECURE                               :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMRAWDMA_ROOT_PDO_R1_GSECURE                               :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMRAWDMA_ROOT_AEO_R1_GSECURE                               :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMRAWDMA_ROOT_AEHO_R1_GSECURE                              :  1;    /*  6.. 6, 0x00000040 */
				FIELD CAMRAWDMA_ROOT_AWBO_R1_GSECURE                              :  1;    /*  7.. 7, 0x00000080 */
				FIELD CAMRAWDMA_ROOT_AWBO_R2_GSECURE                              :  1;    /*  8.. 8, 0x00000100 */
				FIELD CAMRAWDMA_ROOT_AFO_R1_GSECURE                               :  1;    /*  9.. 9, 0x00000200 */
				FIELD CAMRAWDMA_ROOT_LTMSBO_R1_GSECURE                            :  1;    /* 10..10, 0x00000400 */
				FIELD CAMRAWDMA_ROOT_LTMSGO_R1_GSECURE                            :  1;    /* 11..11, 0x00000800 */
				FIELD CAMRAWDMA_ROOT_DRZB2NO_R1_GSECURE                           :  1;    /* 12..12, 0x00001000 */
				FIELD CAMRAWDMA_ROOT_DRZB2NBO_R1_GSECURE                          :  1;    /* 13..13, 0x00002000 */
				FIELD CAMRAWDMA_ROOT_DRZB2NCO_R1_GSECURE                          :  1;    /* 14..14, 0x00004000 */
				FIELD CAMRAWDMA_ROOT_DRZB2NDO_R1_GSECURE                          :  1;    /* 15..15, 0x00008000 */
				FIELD CAMRAWDMA_ROOT_GMPO_R1_GSECURE                              :  1;    /* 16..16, 0x00010000 */
				FIELD CAMRAWDMA_ROOT_GRMGO_R1_GSECURE                             :  1;    /* 17..17, 0x00020000 */
				FIELD CAMRAWDMA_ROOT_MGGMO_R1_GSECURE                             :  1;    /* 18..18, 0x00040000 */
				FIELD CAMRAWDMA_ROOT_AEDO_R1_GSECURE                              :  1;    /* 19..19, 0x00080000 */
				FIELD CAMRAWDMA_ROOT_DFLKO_R1_GSECURE                             :  1;    /* 20..20, 0x00100000 */
				FIELD CAMRAWDMA_ROOT_DFLKBO_R1_GSECURE                            :  1;    /* 21..21, 0x00200000 */
				FIELD CAMRAWDMA_ROOT_LTMSTO_R1_GSECURE                            :  1;    /* 22..22, 0x00400000 */
				FIELD CAMRAWDMA_ROOT_LTMSTO_R2_GSECURE                            :  1;    /* 23..23, 0x00800000 */
				FIELD CAMRAWDMA_ROOT_TSFSO_R1_GSECURE                             :  1;    /* 24..24, 0x01000000 */
				FIELD CAMRAWDMA_ROOT_TSFSO_R2_GSECURE                             :  1;    /* 25..25, 0x02000000 */
				FIELD CAMRAWDMA_ROOT_TSFSO_R3_GSECURE                             :  1;    /* 26..26, 0x04000000 */
				FIELD CAMRAWDMA_ROOT_TSFSO_R4_GSECURE                             :  1;    /* 27..27, 0x08000000 */
				FIELD CAMRAWDMA_ROOT_STATDISI_R1_GSECURE                          :  1;    /* 28..28, 0x10000000 */
				FIELD CAMRAWDMA_ROOT_STATCOLO_R1_GSECURE                          :  1;    /* 29..29, 0x20000000 */
				FIELD rsv_30                                                      :  2;    /* 30..31, 0xc0000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMRAWDMA_ROOT_SECURE_REGISTER_1;
typedef union {
		struct /* 0x3A801000 */ {
				FIELD CAMCTL3_ROOT_INNER_CID                                      :  5;    /*  0.. 4, 0x0000001f */
				FIELD rsv_5                                                       :  3;    /*  5.. 7, 0x000000e0 */
				FIELD CAMCTL3_ROOT_OUTER_CID                                      :  5;    /*  8..12, 0x00001f00 */
				FIELD rsv_13                                                      : 19;    /* 13..31, 0xffffe000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL3_ROOT_CID;
typedef union {
		struct /* 0x3A801010 */ {
				FIELD CAMCTL3_ROOT_DFLK_R1_STAT_PROT_EN                           :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMCTL3_ROOT_TSFS_R3_STAT_PROT_EN                           :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMCTL3_ROOT_TSFS_R4_STAT_PROT_EN                           :  1;    /*  2.. 2, 0x00000004 */
				FIELD rsv_3                                                       : 29;    /*  3..31, 0xfffffff8 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL3_ROOT_STAT_PROT_EN;
typedef union {
		struct /* 0x3A801014 */ {
				FIELD CAMCTL3_ROOT_DFLK_BLK_SIZE_X                                : 12;    /*  0..11, 0x00000fff */
				FIELD rsv_12                                                      : 20;    /* 12..31, 0xfffff000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL3_ROOT_STAT_PROT_SIZE0;
typedef union {
		struct /* 0x3A801018 */ {
				FIELD CAMCTL3_ROOT_TSFS_W_HSIZE                                   : 10;    /*  0.. 9, 0x000003ff */
				FIELD rsv_10                                                      :  6;    /* 10..15, 0x0000fc00 */
				FIELD CAMCTL3_ROOT_TSFS_W_VSIZE                                   : 10;    /* 16..25, 0x03ff0000 */
				FIELD rsv_26                                                      :  6;    /* 26..31, 0xfc000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL3_ROOT_STAT_PROT_SIZE1;
typedef union {
		struct /* 0x3A822000 */ {
				FIELD CAMCTL2_ROOT_INNER_cid                                      :  5;    /*  0.. 4, 0x0000001f */
				FIELD rsv_5                                                       :  3;    /*  5.. 7, 0x000000e0 */
				FIELD CAMCTL2_ROOT_OUTER_cid                                      :  5;    /*  8..12, 0x00001f00 */
				FIELD rsv_13                                                      : 19;    /* 13..31, 0xffffe000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL2_ROOT_CID;
typedef union {
		struct /* 0x3A822010 */ {
				FIELD CAMCTL2_ROOT_tcys_r1_stat_prot_en                           :  1;    /*  0.. 0, 0x00000001 */
				FIELD rsv_1                                                       : 31;    /*  1..31, 0xfffffffe */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL2_ROOT_STAT_PROT_EN;
typedef union {
		struct /* 0x3A822014 */ {
				FIELD CAMCTL2_ROOT_tcys_w_hsize                                   : 14;    /*  0..13, 0x00003fff */
				FIELD rsv_14                                                      :  2;    /* 14..15, 0x0000c000 */
				FIELD CAMCTL2_ROOT_tcys_w_vsize                                   : 14;    /* 16..29, 0x3fff0000 */
				FIELD rsv_30                                                      :  2;    /* 30..31, 0xc0000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL2_ROOT_STAT_PROT_SIZE0;
typedef union {
		struct /* 0x3A823000 */ {
				FIELD CAMYUVDMA_ROOT_LSID                                         :  4;    /*  0.. 3, 0x0000000f */
				FIELD rsv_4                                                       : 12;    /*  4..15, 0x0000fff0 */
				FIELD CAMYUVDMA_ROOT_UID_SEL                                      :  1;    /* 16..16, 0x00010000 */
				FIELD rsv_17                                                      : 15;    /* 17..31, 0xfffe0000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_SECURE_CTRL;
typedef union {
		struct /* 0x3A823004 */ {
				FIELD CAMYUVDMA_ROOT_YUVO_R1_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMYUVDMA_ROOT_YUVBO_R1_GDOMAIN                             :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMYUVDMA_ROOT_YUVCO_R1_GDOMAIN                             :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMYUVDMA_ROOT_YUVDO_R1_GDOMAIN                             :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_0;
typedef union {
		struct /* 0x3A823008 */ {
				FIELD CAMYUVDMA_ROOT_YUVO_R3_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMYUVDMA_ROOT_YUVBO_R3_GDOMAIN                             :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMYUVDMA_ROOT_YUVCO_R3_GDOMAIN                             :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMYUVDMA_ROOT_YUVDO_R3_GDOMAIN                             :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_1;
typedef union {
		struct /* 0x3A82300C */ {
				FIELD CAMYUVDMA_ROOT_YUVO_R2_GDOMAIN                              :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMYUVDMA_ROOT_YUVBO_R2_GDOMAIN                             :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMYUVDMA_ROOT_YUVO_R4_GDOMAIN                              :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMYUVDMA_ROOT_YUVBO_R4_GDOMAIN                             :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_2;
typedef union {
		struct /* 0x3A823010 */ {
				FIELD CAMYUVDMA_ROOT_DRZH2NO_R1_GDOMAIN                           :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMYUVDMA_ROOT_DRZH2NO_R8_GDOMAIN                           :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMYUVDMA_ROOT_DRZS4NO_R3_GDOMAIN                           :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMYUVDMA_ROOT_RZH1N2TO_R2_GDOMAIN                          :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_3;
typedef union {
		struct /* 0x3A823014 */ {
				FIELD CAMYUVDMA_ROOT_TCYSO_R1_GDOMAIN                             :  8;    /*  0.. 7, 0x000000ff */
				FIELD CAMYUVDMA_ROOT_FHO_R3_GDOMAIN                               :  8;    /*  8..15, 0x0000ff00 */
				FIELD CAMYUVDMA_ROOT_DRZH1NO_R1_GDOMAIN                           :  8;    /* 16..23, 0x00ff0000 */
				FIELD CAMYUVDMA_ROOT_DRZH1NBO_R1_GDOMAIN                          :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_DOMAIN_REGISTER_4;
typedef union {
		struct /* 0x3A823018 */ {
				FIELD CAMYUVDMA_ROOT_YUVO_R1_GSECURE                              :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMYUVDMA_ROOT_YUVBO_R1_GSECURE                             :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMYUVDMA_ROOT_YUVCO_R1_GSECURE                             :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMYUVDMA_ROOT_YUVDO_R1_GSECURE                             :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMYUVDMA_ROOT_YUVO_R3_GSECURE                              :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMYUVDMA_ROOT_YUVBO_R3_GSECURE                             :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMYUVDMA_ROOT_YUVCO_R3_GSECURE                             :  1;    /*  6.. 6, 0x00000040 */
				FIELD CAMYUVDMA_ROOT_YUVDO_R3_GSECURE                             :  1;    /*  7.. 7, 0x00000080 */
				FIELD CAMYUVDMA_ROOT_YUVO_R2_GSECURE                              :  1;    /*  8.. 8, 0x00000100 */
				FIELD CAMYUVDMA_ROOT_YUVBO_R2_GSECURE                             :  1;    /*  9.. 9, 0x00000200 */
				FIELD CAMYUVDMA_ROOT_YUVO_R4_GSECURE                              :  1;    /* 10..10, 0x00000400 */
				FIELD CAMYUVDMA_ROOT_YUVBO_R4_GSECURE                             :  1;    /* 11..11, 0x00000800 */
				FIELD rsv_12                                                      :  2;    /* 12..13, 0x00003000 */
				FIELD CAMYUVDMA_ROOT_DRZH2NO_R1_GSECURE                           :  1;    /* 14..14, 0x00004000 */
				FIELD CAMYUVDMA_ROOT_DRZH2NO_R8_GSECURE                           :  1;    /* 15..15, 0x00008000 */
				FIELD CAMYUVDMA_ROOT_DRZS4NO_R3_GSECURE                           :  1;    /* 16..16, 0x00010000 */
				FIELD CAMYUVDMA_ROOT_RZH1N2TO_R2_GSECURE                          :  1;    /* 17..17, 0x00020000 */
				FIELD CAMYUVDMA_ROOT_DRZH1NO_R1_GSECURE                           :  1;    /* 18..18, 0x00040000 */
				FIELD CAMYUVDMA_ROOT_DRZH1NBO_R1_GSECURE                          :  1;    /* 19..19, 0x00080000 */
				FIELD rsv_20                                                      :  2;    /* 20..21, 0x00300000 */
				FIELD CAMYUVDMA_ROOT_TCYSO_R1_GSECURE                             :  1;    /* 22..22, 0x00400000 */
				FIELD CAMYUVDMA_ROOT_FHO_R3_GSECURE                               :  1;    /* 23..23, 0x00800000 */
				FIELD rsv_24                                                      :  8;    /* 24..31, 0xff000000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMYUVDMA_ROOT_SECURE_REGISTER_0;
typedef union {
		struct /* 0x3A7001C0 */ {
				FIELD CAMCTL_SEP_I_TIF_DL_EN                                      :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMCTL_SEP_I2_TIF_DL_EN                                     :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMCTL_SRMG_SRC_I_TIF_DL_EN                                 :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMCTL_ADL_A_O_TIF_DL_EN                                    :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMCTL_MRG_R4_I_TIF_DL_EN                                   :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMCTL_MRG_R6_I_TIF_DL_EN                                   :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMCTL_PMRG_R6_I_TIF_DL_EN                                  :  1;    /*  6.. 6, 0x00000040 */
				FIELD CAMCTL_PMRG_R7_I_TIF_DL_EN                                  :  1;    /*  7.. 7, 0x00000080 */
				FIELD CAMCTL_DRZB2N_O_TIF_DL_EN                                   :  1;    /*  8.. 8, 0x00000100 */
				FIELD CAMCTL_DRZB2N_BO_TIF_DL_EN                                  :  1;    /*  9.. 9, 0x00000200 */
				FIELD CAMCTL_DRZB2N_CO_TIF_DL_EN                                  :  1;    /* 10..10, 0x00000400 */
				FIELD CAMCTL_DRZB2N_DO_TIF_DL_EN                                  :  1;    /* 11..11, 0x00000800 */
				FIELD CAMCTL_ADL_EXP_SRC_I_TIF_DL_EN                              :  1;    /* 12..12, 0x00001000 */
				FIELD CAMCTL_PMRG_R8_I_TIF_DL_EN                                  :  1;    /* 13..13, 0x00002000 */
				FIELD CAMCTL_PMRG_R10_I_TIF_DL_EN                                 :  1;    /* 14..14, 0x00004000 */
				FIELD rsv_15                                                      : 17;    /* 15..31, 0xffff8000 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_TIF_DL_EN;
typedef union {
		struct /* 0x3A7001C4 */ {
				FIELD rsv_0                                                       :  2;    /*  0.. 1, 0x00000003 */
				FIELD CAMCTL_DCIF_DL_EN                                           :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMCTL_SENINF_DL_EN                                         :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMCTL_SEP_I_CTL_DL_EN                                      :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMCTL_SEP_I2_CTL_DL_EN                                     :  1;    /*  5.. 5, 0x00000020 */
				FIELD CAMCTL_SEP_R5_I_CTL_DL_EN                                   :  1;    /*  6.. 6, 0x00000040 */
				FIELD CAMCTL_SEP_R5_I2_CTL_DL_EN                                  :  1;    /*  7.. 7, 0x00000080 */
				FIELD rsv_8                                                       : 24;    /*  8..31, 0xffffff00 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL_OTR_DL_EN;
typedef union {
		struct /* 0x3A7401C0 */ {
				FIELD CAMCTL3_FUS_SRC_I_TIF_DL_EN                                 :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMCTL3_PMRG_R9_I_TIF_DL_EN                                 :  1;    /*  1.. 1, 0x00000002 */
				FIELD rsv_2                                                       : 30;    /*  2..31, 0xfffffffc */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL3_TIF_DL_EN;
typedef union {
		struct /* 0x3A7801C0 */ {
				FIELD CAMCTL2_MRG_R5_I_TIF_DL_EN                                  :  1;    /*  0.. 0, 0x00000001 */
				FIELD CAMCTL2_PMRG_R1_I_TIF_DL_EN                                 :  1;    /*  1.. 1, 0x00000002 */
				FIELD CAMCTL2_PMRG_R2_I_TIF_DL_EN                                 :  1;    /*  2.. 2, 0x00000004 */
				FIELD CAMCTL2_PMRG_R3_I_TIF_DL_EN                                 :  1;    /*  3.. 3, 0x00000008 */
				FIELD CAMCTL2_PMRG_R4_I_TIF_DL_EN                                 :  1;    /*  4.. 4, 0x00000010 */
				FIELD CAMCTL2_PMRG_R5_I_TIF_DL_EN                                 :  1;    /*  5.. 5, 0x00000020 */
				FIELD rsv_6                                                       : 26;    /*  6..31, 0xffffffc0 */
		} Bits;
		UINT32 Raw;
} REG_R_CAMCTL2_TIF_DL_EN;

int isp_sec_configCam_platform(int bSecure, int cam_id);
int isp_sec_configCamsv_platform(int bSecure, int cam_id);
int isp_sec_streamOn_platform(int without_tg, int CamModule);

#endif
