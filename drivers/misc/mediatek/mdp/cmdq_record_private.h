/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __CMDQ_RECORD_PRIVATE_H__
#define __CMDQ_RECORD_PRIVATE_H__

#include "cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

s32 cmdq_append_command(struct cmdqRecStruct *handle,
	enum cmdq_code code,
	u32 arg_a, u32 arg_b, u32 arg_a_type, u32 arg_b_type);
s32 cmdq_op_finalize_command(struct cmdqRecStruct *handle, bool loop);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_RECORD_PRIVATE_H__ */
