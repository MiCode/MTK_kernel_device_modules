// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_HELPER_H__
#define __GPUEB_HELPER_H__

#define GPUEB_TAG   "[GPU/EB]"
#define GHPM_TAG    "[GPU/GHPM]"

#define gpueb_pr_err(tag, fmt, args...) \
	pr_err(tag"[%s:%d]: "fmt"\n", __func__, __LINE__, ##args)
#define gpueb_pr_info(tag, fmt, args...) \
	pr_info(tag"[%s:%d]: "fmt"\n", __func__, __LINE__, ##args)
#define gpueb_pr_debug(tag, fmt, args...) \
	pr_debug(tag"[%s:%d]: "fmt"\n", __func__, __LINE__, ##args)

#define gpueb_pr_logbuf(tag, buf, len, size, fmt, args...) \
	{ \
		pr_info(tag"@%s: "fmt"\n", __func__, ##args); \
		if (buf && len) \
			*len += snprintf(buf + *len, size - *len, fmt"\n", ##args); \
	}

#endif /* __GPUEB_HELPER_H__ */