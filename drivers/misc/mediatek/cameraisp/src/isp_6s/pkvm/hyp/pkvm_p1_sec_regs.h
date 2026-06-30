/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

/*******************************************************************************
 * Defines
 ******************************************************************************/

#define MM_DOMAIN            12

#define ISP_DRV_REG_BASE_A 0x1A030000
#define ISP_DRV_REG_BASE_B 0x1A050000
#define CAM_CAMSYS_SECURE  0x500

#define CAMA_IMGO CAM_IMGO_R1_A
#define CAMA_RRZO CAM_RRZO_R1_A
#define CAMA_UFDI CAM_UFDI_R2_A
#define CAMA_RAWI CAM_RAWI_R2_A
#define CAMA_RSSO CAM_RSSO_R1_A

#define CAMB_IMGO CAM_IMGO_R1_B
#define CAMB_RRZO CAM_RRZO_R1_B
#define CAMB_UFDI CAM_UFDI_R2_B
#define CAMB_RAWI CAM_RAWI_R2_B
#define CAMB_RSSO CAM_RSSO_R1_B

#define SECURE_SMI_PORT_RAWI_R2_OFFSET 0x5040
#define SECURE_SMI_PORT_RAWI_R3_OFFSET 0x5044
#define SECURE_SMI_PORT_UFDI_R2_OFFSET 0x5050
#define SECURE_SMI_PORT_IMGO_R1_OFFSET 0x5054
#define SECURE_SMI_PORT_RRZO_R1_OFFSET 0x5058
#define SECURE_SMI_PORT_RSSO_R1_OFFSET 0x5070
#define SECURE_SMI_PORT_CRZO_R1_OFFSET 0X5078
#define SECURE_SMI_PORT_YUVO_R1_OFFSET 0X507C

#define LCES_OUT_WD_LIMIT 0x280
#define LCES_OUT_HT_LIMIT 0x1E0


union REG_CAMCTL_R1_CAMCTL_DMA_EN {
	struct /* 0x1A030014 */
	{
		unsigned int  CAMCTL_IMGO_R1_EN                                     :  1;      /*  0.. 0, 0x00000001 */
		unsigned int  CAMCTL_UFEO_R1_EN                                     :  1;      /*  1.. 1, 0x00000002 */
		unsigned int  CAMCTL_RRZO_R1_EN                                     :  1;      /*  2.. 2, 0x00000004 */
		unsigned int  CAMCTL_UFGO_R1_EN                                     :  1;      /*  3.. 3, 0x00000008 */
		unsigned int  CAMCTL_YUVO_R1_EN                                     :  1;      /*  4.. 4, 0x00000010 */
		unsigned int  CAMCTL_YUVBO_R1_EN                                    :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  CAMCTL_YUVCO_R1_EN                                    :  1;      /*  6.. 6, 0x00000040 */
		unsigned int  CAMCTL_TSFSO_R1_EN                                    :  1;      /*  7.. 7, 0x00000080 */
		unsigned int  CAMCTL_AAO_R1_EN                                      :  1;      /*  8.. 8, 0x00000100 */
		unsigned int  CAMCTL_AAHO_R1_EN                                     :  1;      /*  9.. 9, 0x00000200 */
		unsigned int  CAMCTL_AFO_R1_EN                                      :  1;      /* 10..10, 0x00000400 */
		unsigned int  CAMCTL_PDO_R1_EN                                      :  1;      /* 11..11, 0x00000800 */
		unsigned int  CAMCTL_FLKO_R1_EN                                     :  1;      /* 12..12, 0x00001000 */
		unsigned int  CAMCTL_LCESO_R1_EN                                    :  1;      /* 13..13, 0x00002000 */
		unsigned int  CAMCTL_LCESHO_R1_EN                                   :  1;      /* 14..14, 0x00004000 */
		unsigned int  CAMCTL_LTMSO_R1_EN                                    :  1;      /* 15..15, 0x00008000 */
		unsigned int  CAMCTL_LMVO_R1_EN                                     :  1;      /* 16..16, 0x00010000 */
		unsigned int  CAMCTL_RSSO_R1_EN                                     :  1;      /* 17..17, 0x00020000 */
		unsigned int  CAMCTL_RSSO_R2_EN                                     :  1;      /* 18..18, 0x00040000 */
		unsigned int  CAMCTL_CRZO_R1_EN                                     :  1;      /* 19..19, 0x00080000 */
		unsigned int  CAMCTL_CRZBO_R1_EN                                    :  1;      /* 20..20, 0x00100000 */
		unsigned int  CAMCTL_CRZO_R2_EN                                     :  1;      /* 21..21, 0x00200000 */
		unsigned int  rsv_22                                                : 10;      /* 22..31, 0xFFC00000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMCTL_R1_CAMCTL_DMA2_EN {
	struct /* 0x1A030018 */
	{
		unsigned int  CAMCTL_RAWI_R2_EN                                     :  1;      /*  0.. 0, 0x00000001 */
		unsigned int  CAMCTL_UFDI_R2_EN                                     :  1;      /*  1.. 1, 0x00000002 */
		unsigned int  CAMCTL_BPCI_R1_EN                                     :  1;      /*  2.. 2, 0x00000004 */
		unsigned int  CAMCTL_LSCI_R1_EN                                     :  1;      /*  3.. 3, 0x00000008 */
		unsigned int  CAMCTL_PDI_R1_EN                                      :  1;      /*  4.. 4, 0x00000010 */
		unsigned int  CAMCTL_BPCI_R2_EN                                     :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  CAMCTL_RAWI_R3_EN                                     :  1;      /*  6.. 6, 0x00000040 */
		unsigned int  CAMCTL_BPCI_R3_EN                                     :  1;      /*  7.. 7, 0x00000080 */
		unsigned int  CAMCTL_CQI_R1_EN                                      :  1;      /*  8.. 8, 0x00000100 */
		unsigned int  CAMCTL_CQI_R2_EN                                      :  1;      /*  9.. 9, 0x00000200 */
		unsigned int  rsv_10                                                : 22;      /* 10..31, 0xFFFFFC00 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMCTL_R1_CAMCTL_SEL {
	struct /* 0x1A030040 */
	{
		unsigned int  CAMCTL_RAW_SEL                                        :  3;      /*  0.. 2, 0x00000007 */
		unsigned int  CAMCTL_SEP_SEL                                        :  1;      /*  3.. 3, 0x00000008 */
		unsigned int  CAMCTL_DGN_SEL                                        :  1;      /*  4.. 4, 0x00000010 */
		unsigned int  rsv_5                                                 :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  CAMCTL_UFEG_SEL                                       :  1;      /*  6.. 6, 0x00000040 */
		unsigned int  CAMCTL_BPC_R2_SEL                                     :  1;      /*  7.. 7, 0x00000080 */
		unsigned int  CAMCTL_LCES_SEL                                       :  1;      /*  8.. 8, 0x00000100 */
		unsigned int  CAMCTL_CRP_R3_SEL                                     :  3;      /*  9..11, 0x00000E00 */
		unsigned int  CAMCTL_IMG_SEL                                        :  2;      /* 12..13, 0x00003000 */
		unsigned int  CAMCTL_IMGO_SEL                                       :  2;      /* 14..15, 0x0000C000 */
		unsigned int  CAMCTL_LTMS_SEL                                       :  1;      /* 16..16, 0x00010000 */
		unsigned int  CAMCTL_AF_SEL                                         :  1;      /* 17..17, 0x00020000 */
		unsigned int  CAMCTL_AA_SEL                                         :  1;      /* 18..18, 0x00040000 */
		unsigned int  CAMCTL_FLK_SEL                                        :  3;      /* 19..21, 0x00380000 */
		unsigned int  CAMCTL_YUFE_R1_SEL                                    :  1;      /* 22..22, 0x00400000 */
		unsigned int  CAMCTL_FRZ_SEL                                        :  1;      /* 23..23, 0x00800000 */
		unsigned int  rsv_24                                                :  1;      /* 24..24, 0x01000000 */
		unsigned int  CAMCTL_CRP_R1_SEL                                     :  1;      /* 25..25, 0x02000000 */
		unsigned int  CAMCTL_HDS_SEL                                        :  1;      /* 26..26, 0x04000000 */
		unsigned int  CAMCTL_MRG_R6_SEL                                     :  3;      /* 27..29, 0x38000000 */
		unsigned int  CAMCTL_MRG_R3_O_SEL                                   :  1;      /* 30..30, 0x40000000 */
		unsigned int  rsv_31                                                :  1;      /* 31..31, 0x80000000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMCTL_R1_CAMCTL_LCES_OUT_SIZE {
	struct /* 0x1A030808 */
	{
		unsigned int  LCES_OUT_HT                                           : 10;      /*  0.. 9, 0x000003FF */
		unsigned int  rsv_10                                                :  6;      /* 10..15, 0x0000FC00 */
		unsigned int  LCES_OUT_WD                                           : 10;      /* 16..25, 0x03FF0000 */
		unsigned int  rsv_26                                                :  6;      /* 26..31, 0xFC000000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RAWI_R2 {
	struct /* 0x1A035040 */
	{
		unsigned int  CAMDMATOP2_RAWI_R2_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_RAWI_R2_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 : 26;      /*  6..31, 0xFFFFFFC0 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RAWI_R3 {
	struct /* 0x1A035044 */
	{
		unsigned int  CAMDMATOP2_RAWI_R3_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_RAWI_R3_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 :  2;      /*  6.. 7, 0x000000C0 */
		unsigned int  CAMDMATOP2_CQI_R1_GDOMAIN                             :  5;      /*  8..12, 0x00001F00 */
		unsigned int  CAMDMATOP2_CQI_R1_GSECURE                             :  1;      /* 13..13, 0x00002000 */
		unsigned int  rsv_14                                                :  2;      /* 14..15, 0x0000C000 */
		unsigned int  CAMDMATOP2_CQI_R2_GDOMAIN                             :  5;      /* 16..20, 0x001F0000 */
		unsigned int  CAMDMATOP2_CQI_R2_GSECURE                             :  1;      /* 21..21, 0x00200000 */
		unsigned int  rsv_22                                                : 10;      /* 22..31, 0xFFC00000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_UFDI_R2 {
	struct /* 0x1A035050 */
	{
		unsigned int  CAMDMATOP2_UFDI_R2_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_UFDI_R2_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 : 26;      /*  6..31, 0xFFFFFFC0 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_IMGO_R1 {
	struct /* 0x1A035054 */
	{
		unsigned int  CAMDMATOP2_IMGO_R1_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_IMGO_R1_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 : 26;      /*  6..31, 0xFFFFFFC0 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RRZO_R1 {
	struct /* 0x1A035058 */
	{
		unsigned int  CAMDMATOP2_RRZO_R1_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_RRZO_R1_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 : 26;      /*  6..31, 0xFFFFFFC0 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_RSSO_R1 {
	struct /* 0x1A035070 */
	{
		unsigned int  CAMDMATOP2_RSSO_R1_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_RSSO_R1_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 :  2;      /*  6.. 7, 0x000000C0 */
		unsigned int  CAMDMATOP2_UFEO_R1_GDOMAIN                            :  5;      /*  8..12, 0x00001F00 */
		unsigned int  CAMDMATOP2_UFEO_R1_GSECURE                            :  1;      /* 13..13, 0x00002000 */
		unsigned int  rsv_14                                                :  2;      /* 14..15, 0x0000C000 */
		unsigned int  CAMDMATOP2_UFGO_R1_GDOMAIN                            :  5;      /* 16..20, 0x001F0000 */
		unsigned int  CAMDMATOP2_UFGO_R1_GSECURE                            :  1;      /* 21..21, 0x00200000 */
		unsigned int  rsv_22                                                :  2;      /* 22..23, 0x00C00000 */
		unsigned int  CAMDMATOP2_RSSO_R2_GDOMAIN                            :  5;      /* 24..28, 0x1F000000 */
		unsigned int  CAMDMATOP2_RSSO_R2_GSECURE                            :  1;      /* 29..29, 0x20000000 */
		unsigned int  rsv_30                                                :  2;      /* 30..31, 0xC0000000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_CRZO_R1 {
	struct /* 0x1A035078 */
	{
		unsigned int  CAMDMATOP2_CRZO_R1_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_CRZO_R1_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 :  2;      /*  6.. 7, 0x000000C0 */
		unsigned int  CAMDMATOP2_CRZBO_R1_GDOMAIN                           :  5;      /*  8..12, 0x00001F00 */
		unsigned int  CAMDMATOP2_CRZBO_R1_GSECURE                           :  1;      /* 13..13, 0x00002000 */
		unsigned int  rsv_14                                                :  2;      /* 14..15, 0x0000C000 */
		unsigned int  CAMDMATOP2_CRZO_R2_GDOMAIN                            :  5;      /* 16..20, 0x001F0000 */
		unsigned int  CAMDMATOP2_CRZO_R2_GSECURE                            :  1;      /* 21..21, 0x00200000 */
		unsigned int  rsv_22                                                : 10;      /* 22..31, 0xFFC00000 */
	} Bits;
	unsigned int Raw;
};

union REG_CAMDMATOP2_R1_CAMDMATOP2_SECURE_SMI_PORT_YUVO_R1 {
	struct /* 0x1A03507C */
	{
		unsigned int  CAMDMATOP2_YUVO_R1_GDOMAIN                            :  5;      /*  0.. 4, 0x0000001F */
		unsigned int  CAMDMATOP2_YUVO_R1_GSECURE                            :  1;      /*  5.. 5, 0x00000020 */
		unsigned int  rsv_6                                                 :  2;      /*  6.. 7, 0x000000C0 */
		unsigned int  CAMDMATOP2_YUVBO_R1_GDOMAIN                           :  5;      /*  8..12, 0x00001F00 */
		unsigned int  CAMDMATOP2_YUVBO_R1_GSECURE                           :  1;      /* 13..13, 0x00002000 */
		unsigned int  rsv_14                                                :  2;      /* 14..15, 0x0000C000 */
		unsigned int  CAMDMATOP2_YUVCO_R1_GDOMAIN                           :  5;      /* 16..20, 0x001F0000 */
		unsigned int  CAMDMATOP2_YUVCO_R1_GSECURE                           :  1;      /* 21..21, 0x00200000 */
		unsigned int  rsv_22                                                : 10;      /* 22..31, 0xFFC00000 */
	} Bits;
	unsigned int Raw;
};
