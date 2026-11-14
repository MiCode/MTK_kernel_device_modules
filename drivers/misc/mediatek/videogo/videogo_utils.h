/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef VIDEOGO_UTILS_H
#define VIDEOGO_UTILS_H

extern bool mtk_vgo_debug;

#define mtk_vgo_debug(format, args...) \
	do { \
		if (mtk_vgo_debug) \
			pr_info("[VGO][DEBUG] %s:%d " format "\n", \
				__func__, __LINE__, ##args); \
	} while (0)

#define mtk_vgo_info(format, args...) \
	pr_info("[VGO][INFO] %s:%d " format "\n", __func__, __LINE__, \
		##args)

#define mtk_vgo_err(format, args...) \
	pr_info("[VGO][ERROR] %s:%d " format "\n", __func__, __LINE__, \
		##args)

#endif // VIDEOGO_UTILS_H
