/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _HWCOMP_BRIDGE_TYPE_H_
#define _HWCOMP_BRIDGE_TYPE_H_

struct comp_pp_info;
struct dcomp_pp_info;
typedef int (*zspool_to_hwcomp_buffer_fn)(struct dcomp_pp_info *obj, void *buffer, unsigned long handle,
					unsigned int slen, unsigned int copysz_aligned);

enum hwcomp_flags;
typedef void (*compress_pp_fn)(int err, void *buffer, unsigned int comp_len, struct comp_pp_info *pp_info,
					enum hwcomp_flags flag);
typedef void (*decompress_pp_fn)(int err, struct dcomp_pp_info *pp_info);

#endif /* _HWCOMP_BRIDGE_TYPE_H_ */
