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

#define MAX_PROC_NAME 8
#define MAX_TGID_LIST 8

static const char *target_names[MAX_PROC_NAME] = {
	"android.hardware.media.c2-mediatek-64b",
	"vdec_worker",
	"vdec_ipi_recv",
	"venc_worker",
	"venc_ipi_recv"
};
static int target_name_count = 5;
static pid_t tgid_list[MAX_TGID_LIST];
static int tgid_count;

void find_tgids_by_names(void);
void cpu_usage_init(void);
void cpu_usage_exit(void);
int get_cpu_usage(int cpu);
#endif // VIDEOGO_UTILS_H
