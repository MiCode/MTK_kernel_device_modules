/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __ISP_SEC_ENTRY_H__
#define __ISP_SEC_ENTRY_H__

#include "pkvm_isp_hyp.h"

ISP_RETURN isp_config_sethsfcam(struct user_pt_regs *regs);
ISP_RETURN isp_config_sethsfcamsv(struct user_pt_regs *regs);
ISP_RETURN isp_stream_ctrl(struct user_pt_regs *regs);

#endif
