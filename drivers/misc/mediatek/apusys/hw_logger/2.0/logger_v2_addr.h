// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __LOGGER_V2_ADDR_H__
#define __LOGGER_V2_ADDR_H__

extern void *apu_logtop, *apu_mbox;

#define APU_LOGTOP_BASE              (apu_logtop)
#define APU_LOG_BUF_W_PTR            (APU_LOGTOP_BASE + 0x80)
#define APU_LOG_BUF_R_PTR            (APU_LOGTOP_BASE + 0x84)
#define APU_LOGTOP_CON_ADDR          (APU_LOGTOP_BASE + 0x0)
#define APU_LOGTOP_CON_SFT           (8)
#define APU_LOGTOP_CON_MSK           (0xF << APU_LOGTOP_CON_SFT)

#define APU_MBOX_BASE                (apu_mbox)
#define LOG_W_OFS_MBOX               (APU_MBOX_BASE + 0x40)

#endif /* __LOGGER_V2_ADDR_H__ */
