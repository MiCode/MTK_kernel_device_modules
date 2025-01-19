/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __SAP_H__
#define __SAP_H__

#include <linux/kconfig.h>

#define CFG_WDT_IRQ_OFFSET		(0x0030)
#define CFG_GPR5_OFFSET			(0x0054)
#define CFG_STATUS_OFFSET		(0x0070)
#define CFG_MON_PC_OFFSET		(0x0080)
#define CFG_MON_LR_OFFSET		(0x0084)
#define CFG_MON_SP_OFFSET		(0x0088)
#define CFG_MON_PC_LATCH_OFFSET		(0x00d0)
#define CFG_MON_LR_LATCH_OFFSET		(0x00d4)
#define CFG_MON_SP_LATCH_OFFSET		(0x00d8)

#define PIN_IN_SIZE_SENSOR_SAP_NOTIFY	(17)
#define PIN_OUT_SIZE_SENSOR_SAP_NOTIFY	(17)

enum {
	IPI_IN_SENSOR_SAP_NOTIFY,
	IPI_OUT_SENSOR_SAP_NOTIFY,
	SAP_IPI_COUNT,
};

extern struct mtk_ipi_device sap_ipidev;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SAP_SUPPORT)
bool sap_enabled(void);
uint8_t sap_get_core_id(void);
uint32_t sap_cfg_reg_read(uint32_t reg_offset);
void sap_dump_last_regs(void);
void sap_show_last_regs(void);
uint32_t sap_print_last_regs(char *buf, uint32_t size);
uint32_t sap_dump_detail_buff(uint8_t *buff, uint32_t size);
void sap_init(void);
void sap_exit(void);
uint32_t sap_get_secure_dump_size(void);
#else
static inline bool sap_enabled(void) { return false; }
static inline uint8_t sap_get_core_id(void) { return 0; };
static inline uint32_t sap_cfg_reg_read(uint32_t reg_offset) { return 0; }
static inline void sap_dump_last_regs(void) {}
static inline void sap_show_last_regs(void) {}
static inline uint32_t sap_print_last_regs(char *buf, uint32_t size) { return 0; }
static inline uint32_t sap_dump_detail_buff(uint8_t *buff,
	uint32_t size) { return 0; }
static inline void sap_init(void) {}
static inline void sap_exit(void) {}
static inline uint32_t sap_get_secure_dump_size(void) { return 0; };
#endif

#endif
