/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_MML_MAT_FW_H__
#define __MTK_MML_MAT_FW_H__

#include <linux/types.h>
#include "mtk-mml-core.h"

extern int mml_mat_fw;
extern int mml_mat_msg;

#define mat_msg(fmt, args...) \
do { \
	if (mml_mat_msg) \
		_mml_log("[mat]" fmt, ##args); \
	else \
		mml_msg("[mat]" fmt, ##args); \
} while (0)

struct mml_ycbcr_mat {
	/* pre-add vector, 9-bit */
	u16 i0, i1, i2;
	/* post-add vector, 9-bit */
	u16 o0, o1, o2;
	/* matrix coefficient, 15-bit */
	u16 c00, c01, c02;
	u16 c10, c11, c12;
	u16 c20, c21, c22;
	/* value format */
	u8 vector_sign_bit;
	u8 vector_precision;
	u8 coef_sign_bit;
	u8 coef_precision;
};

/*
 * mml_mat_dump - dump mml matrix to kernel log
 *
 * @prefix:	log prefix per line
 * @mat:	matrix to dump formatted representation
 */
void mml_mat_dump(const char *prefix, const struct mml_ycbcr_mat *mat);

struct mat_fw_in;
struct mat_fw_out;

/* mat_fw - Matrix firmware calculate settings
 *
 * @in:		struct mat_fw_in contains ycbcr encoding and range information
 * @out:	struct mat_fw_out contains matrix coefficient results
 *
 * Return:	0 success; errno if fail
 */
int mat_fw(struct mat_fw_in *in, struct mat_fw_out *out);

#endif	/* __MTK_MML_MAT_FW_H__ */
