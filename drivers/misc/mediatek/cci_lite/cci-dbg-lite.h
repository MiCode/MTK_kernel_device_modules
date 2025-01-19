/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Derek-HW Lin <derek-hw.lin@mediatek.com> 
 */

#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define csram_write(offs, val)	__raw_writel(val, csram_base + (offs))

#define OFFS_CCI_TBL_MODE 0x0F9C

extern int mtk_eem_init(struct platform_device *pdev);
extern int mtk_devinfo_init(struct platform_device *pdev);
extern int set_dsu_ctrl_debug(unsigned int eas_ctrl, bool debug_enable);
extern int cpufreq_set_cci_mode(unsigned int mode);
unsigned int cpufreq_get_cci_mode(void);
void set_cci_mode(unsigned int mode);
