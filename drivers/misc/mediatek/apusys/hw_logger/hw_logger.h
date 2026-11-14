// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __HW_LOGGER_H__
#define __HW_LOGGER_H__

#include <linux/platform_device.h>

#include "apu.h"
#include "apusys_core.h"

#define HWLOGR_DEV_NAME "apu_hw_logger"
#define HWLOGR_PREFIX "[apusys_logger]"

extern unsigned char g_hw_logger_log_lv;

/* Log level control by g_hw_logger_log_lv */
enum {
	DBG_LOG_OFF = 0,
	DBG_LOG_ERR,
	DBG_LOG_WARN,
	DBG_LOG_INFO,
	DBG_LOG_DEBUG,
};

#define HWLOGR_PRINT(level, string, ...) {\
	pr_info(HWLOGR_PREFIX level " %s:%d " string, __func__, __LINE__, ##__VA_ARGS__); }

#define HWLOGR_ERR_ON  (g_hw_logger_log_lv >= DBG_LOG_ERR)
#define HWLOGR_WARN_ON (g_hw_logger_log_lv >= DBG_LOG_WARN)
#define HWLOGR_INFO_ON (g_hw_logger_log_lv >= DBG_LOG_INFO)
#define HWLOGR_DBG_ON  (g_hw_logger_log_lv >= DBG_LOG_DEBUG)

#define HWLOGR_ERR(string, ...) {\
	if (HWLOGR_ERR_ON) {HWLOGR_PRINT("[error]", string, ##__VA_ARGS__)} }
#define HWLOGR_WARN(string, ...) {\
	if (HWLOGR_WARN_ON) {HWLOGR_PRINT("[warn]", string, ##__VA_ARGS__)} }
#define HWLOGR_INFO(string, ...) {\
	if (HWLOGR_INFO_ON) {HWLOGR_PRINT("[info]", string, ##__VA_ARGS__)} }
#define HWLOGR_DBG(string, ...) {\
	if (HWLOGR_DBG_ON) {HWLOGR_PRINT("[debug]", string, ##__VA_ARGS__)} }

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	#define apusys_logger_exception_aee_warn(module) \
	do { \
		HWLOGR_ERR("APUSYS_RV_EXCEPTION_APUSYS_LOGGER: %s\n", module); \
		aee_kernel_exception("APUSYS_RV_EXCEPTION_APUSYS_LOGGER", \
			"\nCRDISPATCH_KEY:%s\n", module); \
	} while (0)
#else
#define apusys_logger_exception_aee_warn(...)
#endif

struct hw_logger_v1_ops {
	int (*power_on)(void);
	int (*deep_idle_enter_pre)(void);
	int (*deep_idle_enter_post)(void);
	int (*deep_idle_leave)(void);
	int (*dump_tcm_log)(void);
};

struct mtk_apu_logger_ops {
	const struct hw_logger_v1_ops *v1_ops;

	int (*probe)(struct platform_device *pdev);
	int (*remove)(struct platform_device *pdev);
	void (*shutdown)(struct platform_device *pdev);
	int (*config_init)(struct mtk_apu *apu);
	int (*ipi_init)(struct mtk_apu *apu);
	void (*ipi_remove)(struct mtk_apu *apu);
};

struct mtk_apu_logger_platdata {
	struct mtk_apu_logger_ops ops;
};

/**
 * Legacy callback function without APU fast on/off
 */
int hw_logger_power_on(void);
int hw_logger_deep_idle_enter_pre(void);
int hw_logger_deep_idle_enter_post(void);
int hw_logger_deep_idle_leave(void);
/**
* In low power mode, we use APU TCM as log buffer.
* This function will dump APU TCM to mrdump DRAM for debug.
*/
int hw_logger_dump_tcm_log(void);
int hw_logger_config_init(struct mtk_apu *apu);
int hw_logger_ipi_init(struct mtk_apu *apu);
void hw_logger_ipi_remove(struct mtk_apu *apu);

extern const struct mtk_apu_logger_platdata logger_v1_platdata;
extern const struct mtk_apu_logger_platdata logger_v2_platdata;

#endif /* __HW_LOGGER_H__ */
