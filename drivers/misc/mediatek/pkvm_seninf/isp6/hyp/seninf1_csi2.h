/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __seninf1_csi2_REGS_H__
#define __seninf1_csi2_REGS_H__

typedef unsigned int FIELD;
typedef unsigned int UINT32;

typedef union {
	struct {
		FIELD csi2_lane0_en	: 1;
		FIELD csi2_lane1_en	: 1;
		FIELD csi2_lane2_en	: 1;
		FIELD csi2_lane3_en	: 1;
		FIELD rsv_4		: 28;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_EN, *PREG_SENINF_CSI2_EN;

typedef union {
	struct {
		FIELD rg_csi2_cphy_sel			: 1;
		FIELD rg_csi2_ecc_en			: 1;
		FIELD rg_csi2_b2p_en			: 1;
		FIELD rg_csi2_generic_long_packet_en	: 1;
		FIELD rg_csi2_img_packet_en		: 1;
		FIELD rg_csi2_spec_v2p0_sel		: 1;
		FIELD rg_csi2_descramble_en		: 1;
		FIELD rsv_7				: 1;
		FIELD rg_csi2_vs_output_mode		: 1;
		FIELD rg_csi2_vs_ouput_len_sel		: 1;
		FIELD rsv_10				: 2;
		FIELD rg_csi2_hsync_pol			: 1;
		FIELD rg_csi2_vsync_pol			: 1;
		FIELD rsv_14				: 2;
		FIELD rg_csi2_fifo_push_en		: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_OPT, *PREG_SENINF_CSI2_OPT;

typedef union {
	struct {
		FIELD rg_csi2_header_mode	: 8;
		FIELD rg_csi2_header_len	: 3;
		FIELD rsv_11			: 21;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_HDR_MODE_0, *PREG_SENINF_CSI2_HDR_MODE_0;

typedef union {
	struct {
		FIELD rg_csi2_cphy_header_di_pos	: 8;
		FIELD rg_csi2_cphy_header_wc_pos	: 8;
		FIELD rg_csi2_cphy_header_vcx_pos	: 8;
		FIELD rsv_24				: 8;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_HDR_MODE_1, *PREG_SENINF_CSI2_HDR_MODE_1;

typedef union {
	struct {
		FIELD rg_csi2_resync_cycle_cnt		: 5;
		FIELD rsv_5				: 3;
		FIELD rg_csi2_resync_cycle_cnt_opt	: 1;
		FIELD rg_csi2_resync_dataout_opt	: 1;
		FIELD rg_csi2_resync_bypass		: 1;
		FIELD rsv_11				: 21;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_RESYNC_MERGE_CTRL, *PREG_SENINF_CSI2_RESYNC_MERGE_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_dpcm_mode		: 4;
		FIELD rsv_4			: 4;
		FIELD rg_csi2_dt_30_dpcm_en	: 1;
		FIELD rg_csi2_dt_31_dpcm_en	: 1;
		FIELD rg_csi2_dt_32_dpcm_en	: 1;
		FIELD rg_csi2_dt_33_dpcm_en	: 1;
		FIELD rg_csi2_dt_34_dpcm_en	: 1;
		FIELD rg_csi2_dt_35_dpcm_en	: 1;
		FIELD rg_csi2_dt_36_dpcm_en	: 1;
		FIELD rg_csi2_dt_37_dpcm_en	: 1;
		FIELD rg_csi2_dt_2a_dpcm_en	: 1;
		FIELD rsv_17			: 15;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DPCM_CTRL, *PREG_SENINF_CSI2_DPCM_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s0_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s0_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s0_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s0_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S0_DI_CTRL, *PREG_SENINF_CSI2_S0_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s1_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s1_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s1_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s1_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S1_DI_CTRL, *PREG_SENINF_CSI2_S1_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s2_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s2_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s2_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s2_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S2_DI_CTRL, *PREG_SENINF_CSI2_S2_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s3_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s3_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s3_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s3_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S3_DI_CTRL, *PREG_SENINF_CSI2_S3_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s4_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s4_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s4_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s4_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S4_DI_CTRL, *PREG_SENINF_CSI2_S4_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s5_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s5_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s5_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s5_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S5_DI_CTRL, *PREG_SENINF_CSI2_S5_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s6_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s6_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s6_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s6_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S6_DI_CTRL, *PREG_SENINF_CSI2_S6_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_s7_vc_interleave_en	: 1;
		FIELD rsv_1				: 3;
		FIELD rg_csi2_s7_dt_interleave_mode	: 2;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_s7_vc_sel			: 5;
		FIELD rsv_13				: 3;
		FIELD rg_csi2_s7_dt_sel			: 6;
		FIELD rsv_22				: 10;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_S7_DI_CTRL, *PREG_SENINF_CSI2_S7_DI_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_ch0_grp_mode	: 1;
		FIELD rg_csi2_ch0_vsync_bypass	: 1;
		FIELD rsv_2			: 6;
		FIELD rg_csi2_ch0_s0_grp_en	: 1;
		FIELD rg_csi2_ch0_s1_grp_en	: 1;
		FIELD rg_csi2_ch0_s2_grp_en	: 1;
		FIELD rg_csi2_ch0_s3_grp_en	: 1;
		FIELD rg_csi2_ch0_s4_grp_en	: 1;
		FIELD rg_csi2_ch0_s5_grp_en	: 1;
		FIELD rg_csi2_ch0_s6_grp_en	: 1;
		FIELD rg_csi2_ch0_s7_grp_en	: 1;
		FIELD rsv_16			: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_CH0_CTRL, *PREG_SENINF_CSI2_CH0_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_ch1_grp_mode	: 1;
		FIELD rg_csi2_ch1_vsync_bypass	: 1;
		FIELD rsv_2			: 6;
		FIELD rg_csi2_ch1_s0_grp_en	: 1;
		FIELD rg_csi2_ch1_s1_grp_en	: 1;
		FIELD rg_csi2_ch1_s2_grp_en	: 1;
		FIELD rg_csi2_ch1_s3_grp_en	: 1;
		FIELD rg_csi2_ch1_s4_grp_en	: 1;
		FIELD rg_csi2_ch1_s5_grp_en	: 1;
		FIELD rg_csi2_ch1_s6_grp_en	: 1;
		FIELD rg_csi2_ch1_s7_grp_en	: 1;
		FIELD rsv_16			: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_CH1_CTRL, *PREG_SENINF_CSI2_CH1_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_ch2_grp_mode	: 1;
		FIELD rg_csi2_ch2_vsync_bypass	: 1;
		FIELD rsv_2			: 6;
		FIELD rg_csi2_ch2_s0_grp_en	: 1;
		FIELD rg_csi2_ch2_s1_grp_en	: 1;
		FIELD rg_csi2_ch2_s2_grp_en	: 1;
		FIELD rg_csi2_ch2_s3_grp_en	: 1;
		FIELD rg_csi2_ch2_s4_grp_en	: 1;
		FIELD rg_csi2_ch2_s5_grp_en	: 1;
		FIELD rg_csi2_ch2_s6_grp_en	: 1;
		FIELD rg_csi2_ch2_s7_grp_en	: 1;
		FIELD rsv_16			: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_CH2_CTRL, *PREG_SENINF_CSI2_CH2_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_ch3_grp_mode	: 1;
		FIELD rg_csi2_ch3_vsync_bypass	: 1;
		FIELD rsv_2			: 6;
		FIELD rg_csi2_ch3_s0_grp_en	: 1;
		FIELD rg_csi2_ch3_s1_grp_en	: 1;
		FIELD rg_csi2_ch3_s2_grp_en	: 1;
		FIELD rg_csi2_ch3_s3_grp_en	: 1;
		FIELD rg_csi2_ch3_s4_grp_en	: 1;
		FIELD rg_csi2_ch3_s5_grp_en	: 1;
		FIELD rg_csi2_ch3_s6_grp_en	: 1;
		FIELD rg_csi2_ch3_s7_grp_en	: 1;
		FIELD rsv_16			: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_CH3_CTRL, *PREG_SENINF_CSI2_CH3_CTRL;

typedef union {
	struct {
		FIELD rg_csi2_l0_descramble_type0_seed : 16;
		FIELD rg_csi2_l0_descramble_type1_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L0_DESCRAMBLE_SEED_0, *PREG_SENINF_CSI2_L0_DESCRAMBLE_SEED_0;

typedef union {
	struct {
		FIELD rg_csi2_l0_descramble_type2_seed : 16;
		FIELD rg_csi2_l0_descramble_type3_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L0_DESCRAMBLE_SEED_1, *PREG_SENINF_CSI2_L0_DESCRAMBLE_SEED_1;

typedef union {
	struct {
		FIELD rg_csi2_l1_descramble_type0_seed : 16;
		FIELD rg_csi2_l1_descramble_type1_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L1_DESCRAMBLE_SEED_0, *PREG_SENINF_CSI2_L1_DESCRAMBLE_SEED_0;

typedef union {
	struct {
		FIELD rg_csi2_l1_descramble_type2_seed : 16;
		FIELD rg_csi2_l1_descramble_type3_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L1_DESCRAMBLE_SEED_1, *PREG_SENINF_CSI2_L1_DESCRAMBLE_SEED_1;

typedef union {
	struct {
		FIELD rg_csi2_l2_descramble_type0_seed : 16;
		FIELD rg_csi2_l2_descramble_type1_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L2_DESCRAMBLE_SEED_0, *PREG_SENINF_CSI2_L2_DESCRAMBLE_SEED_0;

typedef union {
	struct {
		FIELD rg_csi2_l2_descramble_type2_seed : 16;
		FIELD rg_csi2_l2_descramble_type3_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L2_DESCRAMBLE_SEED_1, *PREG_SENINF_CSI2_L2_DESCRAMBLE_SEED_1;

typedef union {
	struct {
		FIELD rg_csi2_l3_descramble_type0_seed : 16;
		FIELD rg_csi2_l3_descramble_type1_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L3_DESCRAMBLE_SEED_0, *PREG_SENINF_CSI2_L3_DESCRAMBLE_SEED_0;

typedef union {
	struct {
		FIELD rg_csi2_l3_descramble_type2_seed : 16;
		FIELD rg_csi2_l3_descramble_type3_seed : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_L3_DESCRAMBLE_SEED_1, *PREG_SENINF_CSI2_L3_DESCRAMBLE_SEED_1;

typedef union {
	struct {
		FIELD rg_csi2_err_frame_sync_irq_en		: 1;
		FIELD rg_csi2_err_id_irq_en			: 1;
		FIELD rg_csi2_ecc_err_undetected_irq_en		: 1;
		FIELD rg_csi2_ecc_err_corrected_irq_en		: 1;
		FIELD rg_csi2_ecc_err_double_irq_en		: 1;
		FIELD rg_csi2_crc_correct_irq_en		: 1;
		FIELD rg_csi2_crc_err_irq_en			: 1;
		FIELD rg_csi2_err_multi_lane_sync_irq_en	: 1;
		FIELD rg_csi2_fs_receive_irq_en			: 1;
		FIELD rg_csi2_fe_receive_irq_en			: 1;
		FIELD rg_csi2_ls_receive_irq_en			: 1;
		FIELD rg_csi2_le_receive_irq_en			: 1;
		FIELD rg_csi2_gs_receive_irq_en			: 1;
		FIELD rg_csi2_err_lane_resync_irq_en		: 1;
		FIELD rg_csi2_lane_merge_fifo_af_irq_en		: 1;
		FIELD rsv_15					: 1;
		FIELD rg_csi2_err_frame_sync_s0_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s1_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s2_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s3_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s4_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s5_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s6_irq_en		: 1;
		FIELD rg_csi2_err_frame_sync_s7_irq_en		: 1;
		FIELD rg_csi2_resync_fifo_overflow_l0_irq_en	: 1;
		FIELD rg_csi2_resync_fifo_overflow_l1_irq_en	: 1;
		FIELD rg_csi2_resync_fifo_overflow_l2_irq_en	: 1;
		FIELD rg_csi2_resync_fifo_overflow_l3_irq_en	: 1;
		FIELD rg_csi2_async_fifo_overrun_irq_en		: 1;
		FIELD rg_csi2_receive_data_not_enough_irq_en	: 1;
		FIELD rsv_30					: 1;
		FIELD rg_csi2_irq_clr_mode			: 1;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_IRQ_EN, *PREG_SENINF_CSI2_IRQ_EN;

typedef union {
	struct {
		FIELD ro_csi2_err_frame_sync_irq		: 1;
		FIELD ro_csi2_err_id_irq			: 1;
		FIELD ro_csi2_ecc_err_undetected_irq		: 1;
		FIELD ro_csi2_ecc_err_corrected_irq		: 1;
		FIELD ro_csi2_ecc_err_double_irq		: 1;
		FIELD ro_csi2_crc_correct_irq			: 1;
		FIELD ro_csi2_crc_err_irq			: 1;
		FIELD ro_csi2_err_multi_lane_sync_irq		: 1;
		FIELD ro_csi2_fs_receive_irq			: 1;
		FIELD ro_csi2_fe_receive_irq			: 1;
		FIELD ro_csi2_ls_receive_irq			: 1;
		FIELD ro_csi2_le_receive_irq			: 1;
		FIELD ro_csi2_gs_receive_irq			: 1;
		FIELD ro_csi2_err_lane_resync_irq		: 1;
		FIELD ro_csi2_lane_merge_fifo_af_irq		: 1;
		FIELD rsv_15					: 1;
		FIELD ro_csi2_err_frame_sync_s0_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s1_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s2_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s3_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s4_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s5_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s6_irq		: 1;
		FIELD ro_csi2_err_frame_sync_s7_irq		: 1;
		FIELD ro_csi2_resync_fifo_overflow_l0_irq	: 1;
		FIELD ro_csi2_resync_fifo_overflow_l1_irq	: 1;
		FIELD ro_csi2_resync_fifo_overflow_l2_irq	: 1;
		FIELD ro_csi2_resync_fifo_overflow_l3_irq	: 1;
		FIELD ro_csi2_async_fifo_overrun_irq		: 1;
		FIELD ro_csi2_receive_data_not_enough_irq	: 1;
		FIELD rsv_30					: 2;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_IRQ_STATUS, *PREG_SENINF_CSI2_IRQ_STATUS;

typedef union {
	struct {
		FIELD ro_csi2_line_num	: 16;
		FIELD ro_csi2_frame_num	: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_LINE_FRAME_NUM, *PREG_SENINF_CSI2_LINE_FRAME_NUM;

typedef union {
	struct {
		FIELD ro_csi2_packet_dt	: 6;
		FIELD rsv_6		: 2;
		FIELD ro_csi2_packet_vc	: 5;
		FIELD rsv_13		: 3;
		FIELD ro_csi2_packet_wc	: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_PACKET_STATUS, *PREG_SENINF_CSI2_PACKET_STATUS;

typedef union {
	struct {
		FIELD ro_csi2_generic_short_packet_dt	: 6;
		FIELD rsv_6				: 10;
		FIELD ro_csi2_generic_short_packet_wc	: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_GEN_SHORT_PACKET_STATUS, *PREG_SENINF_CSI2_GEN_SHORT_PACKET_STATUS;

typedef union {
	struct {
		FIELD ro_csi2_packet_cnt	: 16;
		FIELD ro_csi2_packet_cnt_buf	: 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_PACKET_CNT_STATUS, *PREG_SENINF_CSI2_PACKET_CNT_STATUS;

typedef union {
	struct {
		FIELD rg_csi2_dbg_sel		: 8;
		FIELD rsv_8			: 8;
		FIELD rg_csi2_dbg_en		: 1;
		FIELD rg_csi2_dbg_packet_cnt_en	: 1;
		FIELD rsv_18			: 14;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DBG_CTRL, *PREG_SENINF_CSI2_DBG_CTRL;

typedef union {
	struct {
		FIELD ro_csi2_dbg_out : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DBG_OUT, *PREG_SENINF_CSI2_DBG_OUT;

typedef union {
	struct {
		FIELD rg_csi2_spare_0	: 8;
		FIELD rsv_8		: 8;
		FIELD rg_csi2_spare_1	: 8;
		FIELD rsv_24		: 8;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_SPARE, *PREG_SENINF_CSI2_SPARE;

typedef union {
	struct {
		FIELD rg_csi2_sof_pulse_width : 16;
		FIELD rg_csi2_eof_pulse_width : 16;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_SOFEOF_PARAMETER, *PREG_SENINF_CSI2_SOFEOF_PARAMETER;

typedef union {
	struct {
		FIELD rg_csi2_raw24like_userdef_dt	: 6;
		FIELD rsv_6				: 2;
		FIELD rg_csi2_userdef_dt_en		: 1;
		FIELD rsv_9				: 7;
		FIELD rg_adas_ctrl_word_en		: 1;
		FIELD rg_user_def_38_2_3f_en		: 1;
		FIELD rsv_18				: 14;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_R24USERDEF_DT, *PREG_SENINF_CSI2_R24USERDEF_DT;

typedef union {
	struct {
		FIELD rg_csi2_raw10_dt	: 6;
		FIELD rsv_6		: 2;
		FIELD rg_csi2_raw12_dt	: 6;
		FIELD rsv_14		: 2;
		FIELD rg_csi2_raw14_dt	: 6;
		FIELD rsv_22		: 2;
		FIELD rg_csi2_raw16_dt	: 6;
		FIELD rsv_30		: 2;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DT0, *PREG_SENINF_CSI2_DT0;

typedef union {
	struct {
		FIELD rg_csi2_raw20_dt		: 6;
		FIELD rsv_6			: 2;
		FIELD rg_csi2_yuv420_10		: 6;
		FIELD rsv_14			: 2;
		FIELD rg_csi2_yuv420csps_10	: 6;
		FIELD rsv_22			: 2;
		FIELD rg_csi2_yuv422_10		: 6;
		FIELD rsv_30			: 2;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DT1, *PREG_SENINF_CSI2_DT1;

typedef union {
	struct {
		FIELD rg_csi2_rgb565_dt	: 6;
		FIELD rsv_6		: 2;
		FIELD rg_csi2_rgb888_dt	: 6;
		FIELD rsv_14		: 2;
		FIELD rg_force_long_pkt	: 1;
		FIELD rg_force_raw8_pkt	: 1;
		FIELD rsv_18		: 14;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DT2, *PREG_SENINF_CSI2_DT2;

typedef union {
	struct {
		FIELD rg_force_dt0	: 6;
		FIELD rsv_6		: 2;
		FIELD rg_force_dt0_sel	: 3;
		FIELD rsv_11		: 1;
		FIELD rg_force_dt0_en	: 1;
		FIELD rsv_13		: 3;
		FIELD rg_force_dt1	: 6;
		FIELD rsv_22		: 2;
		FIELD rg_force_dt1_sel	: 3;
		FIELD rsv_27		: 1;
		FIELD rg_force_dt1_en	: 1;
		FIELD rsv_29		: 3;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_FORCEDT0, *PREG_SENINF_CSI2_FORCEDT0;

typedef union {
	struct {
		FIELD rg_force_dt2	: 6;
		FIELD rsv_6		: 2;
		FIELD rg_force_dt2_sel	: 3;
		FIELD rsv_11		: 1;
		FIELD rg_force_dt2_en	: 1;
		FIELD rsv_13		: 3;
		FIELD rg_force_dt3	: 6;
		FIELD rsv_22		: 2;
		FIELD rg_force_dt3_sel	: 3;
		FIELD rsv_27		: 1;
		FIELD rg_force_dt3_en	: 1;
		FIELD rsv_29		: 3;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_FORCEDT1, *PREG_SENINF_CSI2_FORCEDT1;

typedef union {
	struct {
		FIELD rg_seq_dt0		: 6;
		FIELD rsv_6			: 1;
		FIELD rg_seq10_dt_en		: 1;
		FIELD rg_seq_dt1		: 6;
		FIELD rsv_14			: 1;
		FIELD rg_seq12_dt_en		: 1;
		FIELD rg_seq_dt2		: 6;
		FIELD rsv_22			: 1;
		FIELD rg_seq14_dt_en		: 1;
		FIELD rg_seq_dt3		: 6;
		FIELD rg_seq_dt_38_to_3f_en	: 1;
		FIELD rg_seq_dt_30_to_37_en	: 1;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_FORCEDT2, *PREG_SENINF_CSI2_FORCEDT2;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word00 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD00, *PREG_SENINF_CSI2_DTCTRL_WORD00;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word01 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD01, *PREG_SENINF_CSI2_DTCTRL_WORD01;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word02 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD02, *PREG_SENINF_CSI2_DTCTRL_WORD02;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt0	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT0, *PREG_SENINF_CSI2_DTCTRL_WORDDT0;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word10 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD10, *PREG_SENINF_CSI2_DTCTRL_WORD10;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word11 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD11, *PREG_SENINF_CSI2_DTCTRL_WORD11;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word12 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD12, *PREG_SENINF_CSI2_DTCTRL_WORD12;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt1	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT1, *PREG_SENINF_CSI2_DTCTRL_WORDDT1;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word20 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD20, *PREG_SENINF_CSI2_DTCTRL_WORD20;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word21 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD21, *PREG_SENINF_CSI2_DTCTRL_WORD21;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word22 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD22, *PREG_SENINF_CSI2_DTCTRL_WORD22;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt2	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT2, *PREG_SENINF_CSI2_DTCTRL_WORDDT2;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word30 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD30, *PREG_SENINF_CSI2_DTCTRL_WORD30;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word31 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD31, *PREG_SENINF_CSI2_DTCTRL_WORD31;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word32 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD32, *PREG_SENINF_CSI2_DTCTRL_WORD32;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt3	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT3, *PREG_SENINF_CSI2_DTCTRL_WORDDT3;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word40 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD40, *PREG_SENINF_CSI2_DTCTRL_WORD40;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word41 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD41, *PREG_SENINF_CSI2_DTCTRL_WORD41;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word42 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD42, *PREG_SENINF_CSI2_DTCTRL_WORD42;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt4	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT4, *PREG_SENINF_CSI2_DTCTRL_WORDDT4;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word50 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD50, *PREG_SENINF_CSI2_DTCTRL_WORD50;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word51 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD51, *PREG_SENINF_CSI2_DTCTRL_WORD51;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word52 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD52, *PREG_SENINF_CSI2_DTCTRL_WORD52;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt5	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT5, *PREG_SENINF_CSI2_DTCTRL_WORDDT5;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word60 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD60, *PREG_SENINF_CSI2_DTCTRL_WORD60;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word61 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD61, *PREG_SENINF_CSI2_DTCTRL_WORD61;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word62 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD62, *PREG_SENINF_CSI2_DTCTRL_WORD62;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt6	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT6, *PREG_SENINF_CSI2_DTCTRL_WORDDT6;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word70 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD70, *PREG_SENINF_CSI2_DTCTRL_WORD70;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word71 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD71, *PREG_SENINF_CSI2_DTCTRL_WORD71;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word72 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD72, *PREG_SENINF_CSI2_DTCTRL_WORD72;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_worddt7	: 6;
		FIELD rsv_6			: 26;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORDDT7, *PREG_SENINF_CSI2_DTCTRL_WORDDT7;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word_mask0 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD_MASK0, *PREG_SENINF_CSI2_DTCTRL_WORD_MASK0;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word_mask1 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD_MASK1, *PREG_SENINF_CSI2_DTCTRL_WORD_MASK1;

typedef union {
	struct {
		FIELD rg_csi2_dtctrl_word_mask2 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_DTCTRL_WORD_MASK2, *PREG_SENINF_CSI2_DTCTRL_WORD_MASK2;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word00 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD00, *PREG_SENINF_CSI2_VCCTRL_WORD00;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word01 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD01, *PREG_SENINF_CSI2_VCCTRL_WORD01;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word02 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD02, *PREG_SENINF_CSI2_VCCTRL_WORD02;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc0	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT0, *PREG_SENINF_CSI2_VCCTRL_WORDDT0;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word10 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD10, *PREG_SENINF_CSI2_VCCTRL_WORD10;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word11 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD11, *PREG_SENINF_CSI2_VCCTRL_WORD11;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word12 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD12, *PREG_SENINF_CSI2_VCCTRL_WORD12;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc1	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT1, *PREG_SENINF_CSI2_VCCTRL_WORDDT1;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word20 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD20, *PREG_SENINF_CSI2_VCCTRL_WORD20;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word21 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD21, *PREG_SENINF_CSI2_VCCTRL_WORD21;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word22 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD22, *PREG_SENINF_CSI2_VCCTRL_WORD22;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc2	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT2, *PREG_SENINF_CSI2_VCCTRL_WORDDT2;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word30 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD30, *PREG_SENINF_CSI2_VCCTRL_WORD30;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word31 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD31, *PREG_SENINF_CSI2_VCCTRL_WORD31;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word32 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD32, *PREG_SENINF_CSI2_VCCTRL_WORD32;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc3	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT3, *PREG_SENINF_CSI2_VCCTRL_WORDDT3;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word40 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD40, *PREG_SENINF_CSI2_VCCTRL_WORD40;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word41 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD41, *PREG_SENINF_CSI2_VCCTRL_WORD41;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word42 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD42, *PREG_SENINF_CSI2_VCCTRL_WORD42;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc4	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT4, *PREG_SENINF_CSI2_VCCTRL_WORDDT4;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word50 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD50, *PREG_SENINF_CSI2_VCCTRL_WORD50;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word51 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD51, *PREG_SENINF_CSI2_VCCTRL_WORD51;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word52 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD52, *PREG_SENINF_CSI2_VCCTRL_WORD52;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc5	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT5, *PREG_SENINF_CSI2_VCCTRL_WORDDT5;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word60 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD60, *PREG_SENINF_CSI2_VCCTRL_WORD60;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word61 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD61, *PREG_SENINF_CSI2_VCCTRL_WORD61;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word62 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD62, *PREG_SENINF_CSI2_VCCTRL_WORD62;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc6	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT6, *PREG_SENINF_CSI2_VCCTRL_WORDDT6;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word70 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD70, *PREG_SENINF_CSI2_VCCTRL_WORD70;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word71 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD71, *PREG_SENINF_CSI2_VCCTRL_WORD71;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word72 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD72, *PREG_SENINF_CSI2_VCCTRL_WORD72;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_wordvc7	: 5;
		FIELD rsv_5			: 27;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORDDT7, *PREG_SENINF_CSI2_VCCTRL_WORDDT7;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word_mask0 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD_MASK0, *PREG_SENINF_CSI2_VCCTRL_WORD_MASK0;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word_mask1 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD_MASK1, *PREG_SENINF_CSI2_VCCTRL_WORD_MASK1;

typedef union {
	struct {
		FIELD rg_csi2_vcctrl_word_mask2 : 32;
	} Bits;
	UINT32 Raw;
} REG_SENINF_CSI2_VCCTRL_WORD_MASK2, *PREG_SENINF_CSI2_VCCTRL_WORD_MASK2;
// ----------------- seninf1_csi2 Register Definition -------------------
typedef struct /*0x1A004A00*/
{
	REG_SENINF_CSI2_EN			SENINF_CSI2_EN; // 4A00
	REG_SENINF_CSI2_OPT			SENINF_CSI2_OPT; // 4A04
	REG_SENINF_CSI2_HDR_MODE_0		SENINF_CSI2_HDR_MODE_0; // 4A08
	REG_SENINF_CSI2_HDR_MODE_1		SENINF_CSI2_HDR_MODE_1; // 4A0C
	REG_SENINF_CSI2_RESYNC_MERGE_CTRL	SENINF_CSI2_RESYNC_MERGE_CTRL; // 4A10
	UINT32					rsv_4A14; // 4A14
	REG_SENINF_CSI2_DPCM_CTRL		SENINF_CSI2_DPCM_CTRL; // 4A18
	UINT32					rsv_4A1C; // 4A1C
	REG_SENINF_CSI2_S0_DI_CTRL		SENINF_CSI2_S0_DI_CTRL; // 4A20
	REG_SENINF_CSI2_S1_DI_CTRL		SENINF_CSI2_S1_DI_CTRL; // 4A24
	REG_SENINF_CSI2_S2_DI_CTRL		SENINF_CSI2_S2_DI_CTRL; // 4A28
	REG_SENINF_CSI2_S3_DI_CTRL		SENINF_CSI2_S3_DI_CTRL; // 4A2C
	REG_SENINF_CSI2_S4_DI_CTRL		SENINF_CSI2_S4_DI_CTRL; // 4A30
	REG_SENINF_CSI2_S5_DI_CTRL		SENINF_CSI2_S5_DI_CTRL; // 4A34
	REG_SENINF_CSI2_S6_DI_CTRL		SENINF_CSI2_S6_DI_CTRL; // 4A38
	REG_SENINF_CSI2_S7_DI_CTRL		SENINF_CSI2_S7_DI_CTRL; // 4A3C
	UINT32					rsv_4A40[8]; // 4A40..4A5C
	REG_SENINF_CSI2_CH0_CTRL		SENINF_CSI2_CH0_CTRL; // 4A60
	REG_SENINF_CSI2_CH1_CTRL		SENINF_CSI2_CH1_CTRL; // 4A64
	REG_SENINF_CSI2_CH2_CTRL		SENINF_CSI2_CH2_CTRL; // 4A68
	REG_SENINF_CSI2_CH3_CTRL		SENINF_CSI2_CH3_CTRL; // 4A6C
	UINT32					rsv_4A70[4]; // 4A70..4A7C
	REG_SENINF_CSI2_L0_DESCRAMBLE_SEED_0	SENINF_CSI2_L0_DESCRAMBLE_SEED_0; // 4A80
	REG_SENINF_CSI2_L0_DESCRAMBLE_SEED_1	SENINF_CSI2_L0_DESCRAMBLE_SEED_1; // 4A84
	UINT32					rsv_4A88[2]; // 4A88..4A8C
	REG_SENINF_CSI2_L1_DESCRAMBLE_SEED_0	SENINF_CSI2_L1_DESCRAMBLE_SEED_0; // 4A90
	REG_SENINF_CSI2_L1_DESCRAMBLE_SEED_1	SENINF_CSI2_L1_DESCRAMBLE_SEED_1; // 4A94
	UINT32					rsv_4A98[2]; // 4A98..4A9C
	REG_SENINF_CSI2_L2_DESCRAMBLE_SEED_0	SENINF_CSI2_L2_DESCRAMBLE_SEED_0; // 4AA0
	REG_SENINF_CSI2_L2_DESCRAMBLE_SEED_1	SENINF_CSI2_L2_DESCRAMBLE_SEED_1; // 4AA4
	UINT32					rsv_4AA8[2]; // 4AA8..4AAC
	REG_SENINF_CSI2_L3_DESCRAMBLE_SEED_0	SENINF_CSI2_L3_DESCRAMBLE_SEED_0; // 4AB0
	REG_SENINF_CSI2_L3_DESCRAMBLE_SEED_1	SENINF_CSI2_L3_DESCRAMBLE_SEED_1; // 4AB4
	UINT32					rsv_4AB8[2]; // 4AB8..4ABC
	REG_SENINF_CSI2_IRQ_EN			SENINF_CSI2_IRQ_EN; // 4AC0
	UINT32					rsv_4AC4; // 4AC4
	REG_SENINF_CSI2_IRQ_STATUS		SENINF_CSI2_IRQ_STATUS; // 4AC8
	UINT32					rsv_4ACC; // 4ACC
	REG_SENINF_CSI2_LINE_FRAME_NUM		SENINF_CSI2_LINE_FRAME_NUM; // 4AD0
	REG_SENINF_CSI2_PACKET_STATUS		SENINF_CSI2_PACKET_STATUS; // 4AD4
	REG_SENINF_CSI2_GEN_SHORT_PACKET_STATUS	SENINF_CSI2_GEN_SHORT_PACKET_STATUS; // 4AD8
	REG_SENINF_CSI2_PACKET_CNT_STATUS	SENINF_CSI2_PACKET_CNT_STATUS; // 4ADC
	REG_SENINF_CSI2_DBG_CTRL		SENINF_CSI2_DBG_CTRL; // 4AE0
	UINT32					rsv_4AE4[4]; // 4AE4..4AF0
	REG_SENINF_CSI2_DBG_OUT			SENINF_CSI2_DBG_OUT; // 4AF4
	REG_SENINF_CSI2_SPARE			SENINF_CSI2_SPARE; // 4AF8
	REG_SENINF_CSI2_SOFEOF_PARAMETER	SENINF_CSI2_SOFEOF_PARAMETER; // 4AFC
	REG_SENINF_CSI2_R24USERDEF_DT		SENINF_CSI2_R24USERDEF_DT; // 4B00
	REG_SENINF_CSI2_DT0			SENINF_CSI2_DT0; // 4B04
	REG_SENINF_CSI2_DT1			SENINF_CSI2_DT1; // 4B08
	REG_SENINF_CSI2_DT2			SENINF_CSI2_DT2; // 4B0C
	REG_SENINF_CSI2_FORCEDT0		SENINF_CSI2_FORCEDT0; // 4B10
	REG_SENINF_CSI2_FORCEDT1		SENINF_CSI2_FORCEDT1; // 4B14
	REG_SENINF_CSI2_FORCEDT2		SENINF_CSI2_FORCEDT2; // 4B18
	UINT32					rsv_4B1C; // 4B1C
	REG_SENINF_CSI2_DTCTRL_WORD00		SENINF_CSI2_DTCTRL_WORD00; // 4B20
	REG_SENINF_CSI2_DTCTRL_WORD01		SENINF_CSI2_DTCTRL_WORD01; // 4B24
	REG_SENINF_CSI2_DTCTRL_WORD02		SENINF_CSI2_DTCTRL_WORD02; // 4B28
	REG_SENINF_CSI2_DTCTRL_WORDDT0		SENINF_CSI2_DTCTRL_WORDDT0; // 4B2C
	REG_SENINF_CSI2_DTCTRL_WORD10		SENINF_CSI2_DTCTRL_WORD10; // 4B30
	REG_SENINF_CSI2_DTCTRL_WORD11		SENINF_CSI2_DTCTRL_WORD11; // 4B34
	REG_SENINF_CSI2_DTCTRL_WORD12		SENINF_CSI2_DTCTRL_WORD12; // 4B38
	REG_SENINF_CSI2_DTCTRL_WORDDT1		SENINF_CSI2_DTCTRL_WORDDT1; // 4B3C
	REG_SENINF_CSI2_DTCTRL_WORD20		SENINF_CSI2_DTCTRL_WORD20; // 4B40
	REG_SENINF_CSI2_DTCTRL_WORD21		SENINF_CSI2_DTCTRL_WORD21; // 4B44
	REG_SENINF_CSI2_DTCTRL_WORD22		SENINF_CSI2_DTCTRL_WORD22; // 4B48
	REG_SENINF_CSI2_DTCTRL_WORDDT2		SENINF_CSI2_DTCTRL_WORDDT2; // 4B4C
	REG_SENINF_CSI2_DTCTRL_WORD30		SENINF_CSI2_DTCTRL_WORD30; // 4B50
	REG_SENINF_CSI2_DTCTRL_WORD31		SENINF_CSI2_DTCTRL_WORD31; // 4B54
	REG_SENINF_CSI2_DTCTRL_WORD32		SENINF_CSI2_DTCTRL_WORD32; // 4B58
	REG_SENINF_CSI2_DTCTRL_WORDDT3		SENINF_CSI2_DTCTRL_WORDDT3; // 4B5C
	REG_SENINF_CSI2_DTCTRL_WORD40		SENINF_CSI2_DTCTRL_WORD40; // 4B60
	REG_SENINF_CSI2_DTCTRL_WORD41		SENINF_CSI2_DTCTRL_WORD41; // 4B64
	REG_SENINF_CSI2_DTCTRL_WORD42		SENINF_CSI2_DTCTRL_WORD42; // 4B68
	REG_SENINF_CSI2_DTCTRL_WORDDT4		SENINF_CSI2_DTCTRL_WORDDT4; // 4B6C
	REG_SENINF_CSI2_DTCTRL_WORD50		SENINF_CSI2_DTCTRL_WORD50; // 4B70
	REG_SENINF_CSI2_DTCTRL_WORD51		SENINF_CSI2_DTCTRL_WORD51; // 4B74
	REG_SENINF_CSI2_DTCTRL_WORD52		SENINF_CSI2_DTCTRL_WORD52; // 4B78
	REG_SENINF_CSI2_DTCTRL_WORDDT5		SENINF_CSI2_DTCTRL_WORDDT5; // 4B7C
	REG_SENINF_CSI2_DTCTRL_WORD60		SENINF_CSI2_DTCTRL_WORD60; // 4B80
	REG_SENINF_CSI2_DTCTRL_WORD61		SENINF_CSI2_DTCTRL_WORD61; // 4B84
	REG_SENINF_CSI2_DTCTRL_WORD62		SENINF_CSI2_DTCTRL_WORD62; // 4B88
	REG_SENINF_CSI2_DTCTRL_WORDDT6		SENINF_CSI2_DTCTRL_WORDDT6; // 4B8C
	REG_SENINF_CSI2_DTCTRL_WORD70		SENINF_CSI2_DTCTRL_WORD70; // 4B90
	REG_SENINF_CSI2_DTCTRL_WORD71		SENINF_CSI2_DTCTRL_WORD71; // 4B94
	REG_SENINF_CSI2_DTCTRL_WORD72		SENINF_CSI2_DTCTRL_WORD72; // 4B98
	REG_SENINF_CSI2_DTCTRL_WORDDT7		SENINF_CSI2_DTCTRL_WORDDT7; // 4B9C
	REG_SENINF_CSI2_DTCTRL_WORD_MASK0	SENINF_CSI2_DTCTRL_WORD_MASK0; // 4BA0
	REG_SENINF_CSI2_DTCTRL_WORD_MASK1	SENINF_CSI2_DTCTRL_WORD_MASK1; // 4BA4
	REG_SENINF_CSI2_DTCTRL_WORD_MASK2	SENINF_CSI2_DTCTRL_WORD_MASK2; // 4BA8
	UINT32					rsv_4BAC[29]; // 4BAC..4C1C
	REG_SENINF_CSI2_VCCTRL_WORD00		SENINF_CSI2_VCCTRL_WORD00; // 4C20
	REG_SENINF_CSI2_VCCTRL_WORD01		SENINF_CSI2_VCCTRL_WORD01; // 4C24
	REG_SENINF_CSI2_VCCTRL_WORD02		SENINF_CSI2_VCCTRL_WORD02; // 4C28
	REG_SENINF_CSI2_VCCTRL_WORDDT0		SENINF_CSI2_VCCTRL_WORDDT0; // 4C2C
	REG_SENINF_CSI2_VCCTRL_WORD10		SENINF_CSI2_VCCTRL_WORD10; // 4C30
	REG_SENINF_CSI2_VCCTRL_WORD11		SENINF_CSI2_VCCTRL_WORD11; // 4C34
	REG_SENINF_CSI2_VCCTRL_WORD12		SENINF_CSI2_VCCTRL_WORD12; // 4C38
	REG_SENINF_CSI2_VCCTRL_WORDDT1		SENINF_CSI2_VCCTRL_WORDDT1; // 4C3C
	REG_SENINF_CSI2_VCCTRL_WORD20		SENINF_CSI2_VCCTRL_WORD20; // 4C40
	REG_SENINF_CSI2_VCCTRL_WORD21		SENINF_CSI2_VCCTRL_WORD21; // 4C44
	REG_SENINF_CSI2_VCCTRL_WORD22		SENINF_CSI2_VCCTRL_WORD22; // 4C48
	REG_SENINF_CSI2_VCCTRL_WORDDT2		SENINF_CSI2_VCCTRL_WORDDT2; // 4C4C
	REG_SENINF_CSI2_VCCTRL_WORD30		SENINF_CSI2_VCCTRL_WORD30; // 4C50
	REG_SENINF_CSI2_VCCTRL_WORD31		SENINF_CSI2_VCCTRL_WORD31; // 4C54
	REG_SENINF_CSI2_VCCTRL_WORD32		SENINF_CSI2_VCCTRL_WORD32; // 4C58
	REG_SENINF_CSI2_VCCTRL_WORDDT3		SENINF_CSI2_VCCTRL_WORDDT3; // 4C5C
	REG_SENINF_CSI2_VCCTRL_WORD40		SENINF_CSI2_VCCTRL_WORD40; // 4C60
	REG_SENINF_CSI2_VCCTRL_WORD41		SENINF_CSI2_VCCTRL_WORD41; // 4C64
	REG_SENINF_CSI2_VCCTRL_WORD42		SENINF_CSI2_VCCTRL_WORD42; // 4C68
	REG_SENINF_CSI2_VCCTRL_WORDDT4		SENINF_CSI2_VCCTRL_WORDDT4; // 4C6C
	REG_SENINF_CSI2_VCCTRL_WORD50		SENINF_CSI2_VCCTRL_WORD50; // 4C70
	REG_SENINF_CSI2_VCCTRL_WORD51		SENINF_CSI2_VCCTRL_WORD51; // 4C74
	REG_SENINF_CSI2_VCCTRL_WORD52		SENINF_CSI2_VCCTRL_WORD52; // 4C78
	REG_SENINF_CSI2_VCCTRL_WORDDT5		SENINF_CSI2_VCCTRL_WORDDT5; // 4C7C
	REG_SENINF_CSI2_VCCTRL_WORD60		SENINF_CSI2_VCCTRL_WORD60; // 4C80
	REG_SENINF_CSI2_VCCTRL_WORD61		SENINF_CSI2_VCCTRL_WORD61; // 4C84
	REG_SENINF_CSI2_VCCTRL_WORD62		SENINF_CSI2_VCCTRL_WORD62; // 4C88
	REG_SENINF_CSI2_VCCTRL_WORDDT6		SENINF_CSI2_VCCTRL_WORDDT6; // 4C8C
	REG_SENINF_CSI2_VCCTRL_WORD70		SENINF_CSI2_VCCTRL_WORD70; // 4C90
	REG_SENINF_CSI2_VCCTRL_WORD71		SENINF_CSI2_VCCTRL_WORD71; // 4C94
	REG_SENINF_CSI2_VCCTRL_WORD72		SENINF_CSI2_VCCTRL_WORD72; // 4C98
	REG_SENINF_CSI2_VCCTRL_WORDDT7		SENINF_CSI2_VCCTRL_WORDDT7; // 4C9C
	REG_SENINF_CSI2_VCCTRL_WORD_MASK0	SENINF_CSI2_VCCTRL_WORD_MASK0; // 4CA0
	REG_SENINF_CSI2_VCCTRL_WORD_MASK1	SENINF_CSI2_VCCTRL_WORD_MASK1; // 4CA4
	REG_SENINF_CSI2_VCCTRL_WORD_MASK2	SENINF_CSI2_VCCTRL_WORD_MASK2; // 4CA8
}seninf1_csi2_REGS, *Pseninf1_csi2_REGS;

#endif // __seninf1_csi2_REGS_H__
