// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __LOGGER_V2_PROCFS_H__
#define __LOGGER_V2_PROCFS_H__

#define CLEAR_LOG_CMD           "clear"
#define APUSYS_HWLOG_WQ_NAME    "apusys_hwlog_wq"
#define APUSYS_HWLOGR_DIR       "apusys_logger"
#define DEFAULT_SEQ_BUF_SIZE    (1024 * 1024 * 8) // 8MB
#define MAX_RV_INFO_SIZE        (1024 * 1024) // 1MB
#define PROC_WRITE_BUFSIZE      (64)
#define WAIT_LOG_INTERVAL_MIN   (1000)
#define WAIT_LOG_INTERVAL_MAX   (1500)
#define MB_LOG_WAIT_TIMEOUT     (2)
#define BLOCK_LOG_WAIT_TIMEOUT  (UINT_MAX)
#define HWLOG_BINARY_HEADER     (0xA5)

int logger_v2_create_procfs(struct platform_device *pdev);
int logger_v2_remove_procfs(struct platform_device *pdev);
void logger_v2_notify_mblog(unsigned int ms);

#endif