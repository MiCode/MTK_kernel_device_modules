/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Derek-HW Lin <derek-hw.lin@mediatek.com>
 */


extern int set_dsu_ctrl_debug(unsigned int eas_ctrl, bool debug_enable);
extern int cpufreq_set_cci_mode(unsigned int mode);
