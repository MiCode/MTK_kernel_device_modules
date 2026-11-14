/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <KY.Liu@mediatek.com>
 */
#ifndef __CLKBUF_API_H
#define __CLKBUF_API_H

int clkbuf_xo_ctrl(char *cmd, int xo_id, u32 input);
int clkbuf_srclken_ctrl(char *cmd, int sub_id);

#endif
