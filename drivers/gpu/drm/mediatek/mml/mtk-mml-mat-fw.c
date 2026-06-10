// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>

#include "mtk-mml-mat-fw.h"

int mml_mat_fw;
module_param(mml_mat_fw, int, 0644);
int mml_mat_msg;
module_param(mml_mat_msg, int, 0644);

static inline u16 _neg(u16 n, u16 b)
{
	return n >> (b - 1) & 1;
}

static inline u16 _abs(u16 s, u16 n, u16 b)
{
	return s ? (1 << b) - n : n;
}

static inline s32 _sn(u16 s, u16 n)
{
	return s ? -n : n;
}

static inline char _s(u16 s)
{
	return s ? '-' : ' ';
}

static inline u32 _i(u16 n, u16 p)
{
	return n >> p;
}

static inline u32 _d(u32 n, u32 p, u32 d)
{
	return (n & (1 << p) - 1) * d >> p;
}

void mml_mat_dump(const char *prefix, const struct mml_ycbcr_mat *m)
{
	struct mml_ycbcr_mat s, n;

	s.i0 = _neg(m->i0, m->vector_sign_bit);
	s.i1 = _neg(m->i1, m->vector_sign_bit);
	s.i2 = _neg(m->i2, m->vector_sign_bit);
	s.o0 = _neg(m->o0, m->vector_sign_bit);
	s.o1 = _neg(m->o1, m->vector_sign_bit);
	s.o2 = _neg(m->o2, m->vector_sign_bit);
	s.c00 = _neg(m->c00, m->coef_sign_bit);
	s.c01 = _neg(m->c01, m->coef_sign_bit);
	s.c02 = _neg(m->c02, m->coef_sign_bit);
	s.c10 = _neg(m->c10, m->coef_sign_bit);
	s.c11 = _neg(m->c11, m->coef_sign_bit);
	s.c12 = _neg(m->c12, m->coef_sign_bit);
	s.c20 = _neg(m->c20, m->coef_sign_bit);
	s.c21 = _neg(m->c21, m->coef_sign_bit);
	s.c22 = _neg(m->c22, m->coef_sign_bit);
	n.i0 = _abs(s.i0, m->i0, m->vector_sign_bit);
	n.i1 = _abs(s.i1, m->i1, m->vector_sign_bit);
	n.i2 = _abs(s.i2, m->i2, m->vector_sign_bit);
	n.o0 = _abs(s.o0, m->o0, m->vector_sign_bit);
	n.o1 = _abs(s.o1, m->o1, m->vector_sign_bit);
	n.o2 = _abs(s.o2, m->o2, m->vector_sign_bit);
	n.c00 = _abs(s.c00, m->c00, m->coef_sign_bit);
	n.c01 = _abs(s.c01, m->c01, m->coef_sign_bit);
	n.c02 = _abs(s.c02, m->c02, m->coef_sign_bit);
	n.c10 = _abs(s.c10, m->c10, m->coef_sign_bit);
	n.c11 = _abs(s.c11, m->c11, m->coef_sign_bit);
	n.c12 = _abs(s.c12, m->c12, m->coef_sign_bit);
	n.c20 = _abs(s.c20, m->c20, m->coef_sign_bit);
	n.c21 = _abs(s.c21, m->c21, m->coef_sign_bit);
	n.c22 = _abs(s.c22, m->c22, m->coef_sign_bit);

	mat_msg(
		"%sc00=%5d, c01=%5d, c02=%5d, i0=%4d, o0=%4d,  [[%c%d.%05d, %c%d.%05d, %c%d.%05d],   [v0   %c%d.%04d,   [%c%d.%04d,",
		prefix, _sn(s.c00, n.c00), _sn(s.c01, n.c01), _sn(s.c02, n.c02),
		_sn(s.i0, n.i0), _sn(s.o0, n.o0),
		_s(s.c00), _i(n.c00, m->coef_precision), _d(n.c00, m->coef_precision, 100000),
		_s(s.c01), _i(n.c01, m->coef_precision), _d(n.c01, m->coef_precision, 100000),
		_s(s.c02), _i(n.c02, m->coef_precision), _d(n.c02, m->coef_precision, 100000),
		_s(s.i0), _i(n.i0, m->vector_precision), _d(n.i0, m->vector_precision, 10000),
		_s(s.o0), _i(n.o0, m->vector_precision), _d(n.o0, m->vector_precision, 10000));
	mat_msg(
		"%sc10=%5d, c11=%5d, c12=%5d, i1=%4d, o1=%4d,   [%c%d.%05d, %c%d.%05d, %c%d.%05d], x  v1 + %c%d.%04d, +  %c%d.%04d,",
		prefix, _sn(s.c10, n.c10), _sn(s.c11, n.c11), _sn(s.c12, n.c12),
		_sn(s.i1, n.i1), _sn(s.o1, n.o1),
		_s(s.c10), _i(n.c10, m->coef_precision), _d(n.c10, m->coef_precision, 100000),
		_s(s.c11), _i(n.c11, m->coef_precision), _d(n.c11, m->coef_precision, 100000),
		_s(s.c12), _i(n.c12, m->coef_precision), _d(n.c12, m->coef_precision, 100000),
		_s(s.i1), _i(n.i1, m->vector_precision), _d(n.i1, m->vector_precision, 10000),
		_s(s.o1), _i(n.o1, m->vector_precision), _d(n.o1, m->vector_precision, 10000));
	mat_msg(
		"%sc20=%5d, c21=%5d, c22=%5d, i2=%4d, o2=%4d,   [%c%d.%05d, %c%d.%05d, %c%d.%05d]]    v2   %c%d.%04d]    %c%d.%04d]",
		prefix, _sn(s.c20, n.c20), _sn(s.c21, n.c21), _sn(s.c22, n.c22),
		_sn(s.i2, n.i2), _sn(s.o2, n.o2),
		_s(s.c20), _i(n.c20, m->coef_precision), _d(n.c20, m->coef_precision, 100000),
		_s(s.c21), _i(n.c21, m->coef_precision), _d(n.c21, m->coef_precision, 100000),
		_s(s.c22), _i(n.c22, m->coef_precision), _d(n.c22, m->coef_precision, 100000),
		_s(s.i2), _i(n.i2, m->vector_precision), _d(n.i2, m->vector_precision, 10000),
		_s(s.o2), _i(n.o2, m->vector_precision), _d(n.o2, m->vector_precision, 10000));
}

MODULE_DESCRIPTION("MTK MML MAT FW");
MODULE_LICENSE("GPL");
