/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SENINF_TG1_REGS_H__
#define __SENINF_TG1_REGS_H__

// ----------------- SENINF_TG1 Bit Field Definitions -------------------
typedef unsigned int FIELD;
typedef unsigned int UINT32;
typedef unsigned short UINT16;
typedef unsigned char UINT8;

typedef union {
	struct {
		FIELD TM_EN			: 1;
		FIELD TM_RST			: 1;
		FIELD TM_FMT			: 1;
		FIELD TM_BIN_IMG_SWITCH_EN	: 1;
		FIELD TM_PAT			: 5;
		FIELD rsv_9			: 23;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_CTL, *PSENINF_TG1_REG_TM_CTL;

typedef union {
	struct {
		FIELD TM_PXL	: 16;
		FIELD TM_LINE	: 16;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_SIZE, *PSENINF_TG1_REG_TM_SIZE;

typedef union {
	struct {
		FIELD TM_CLK_CNT	: 8;
		FIELD TM_CLRBAR_OFT	: 13;
		FIELD rsv_21		: 7;
		FIELD TM_CLRBAR_IDX	: 3;
		FIELD rsv_31		: 1;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_CLK, *PSENINF_TG1_REG_TM_CLK;

typedef union {
	struct {
		FIELD TM_DUMMYPXL	: 16;
		FIELD TM_VSYNC		: 16;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_DUM, *PSENINF_TG1_REG_TM_DUM;

typedef union {
	struct {
		FIELD TM_SEED : 32;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_RAND_SEED, *PSENINF_TG1_REG_TM_RAND_SEED;

typedef union {
	struct {
		FIELD TM_DIFF_FRM	: 1;
		FIELD rsv_1		: 31;
	} Bits;
	UINT32 Raw;
} SENINF_TG1_REG_TM_RAND_CTL, *PSENINF_TG1_REG_TM_RAND_CTL;

// ----------------- SENINF_TG1 Register Definition -------------------
typedef struct /*0x1a004600*/
{
	UINT32				rsv_4600[2]; // 4600..4604
	SENINF_TG1_REG_TM_CTL		TM_CTL; // 4608
	SENINF_TG1_REG_TM_SIZE		TM_SIZE; // 460C
	SENINF_TG1_REG_TM_CLK		TM_CLK; // 4610
	UINT32				rsv_4614; // 4614
	SENINF_TG1_REG_TM_DUM		TM_DUM; // 4618
	SENINF_TG1_REG_TM_RAND_SEED	TM_RAND_SEED; // 461C
	SENINF_TG1_REG_TM_RAND_CTL	TM_RAND_CTL; // 4620
}SENINF_TG1_REGS, *PSENINF_TG1_REGS;

#endif // __SENINF_TG1_REGS_H__
