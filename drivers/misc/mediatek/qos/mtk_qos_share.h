/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_SHARE_H__
#define __MTK_QOS_SHARE_H__

#define HIST_NUM 8
#define BW_TYPE  4
#define BW_NUM 16
#define SRC_TYPE 4  // EMI_OCC_BW,EMI_DATA_BW,DRAM_OCC_BW,DRAM_DATA_BW,

struct qos_rec_data {
	/* 32 bytes */
	unsigned int rec_version;
	unsigned int reserved[7];

	/* 4 + (8 * 4  * 4) * 2 = 260 bytes */
	unsigned int current_hist;
	unsigned int bw_hist[HIST_NUM][BW_TYPE];
	unsigned int data_bw_hist[HIST_NUM][BW_TYPE];

	/* remaining size = 3804 bytes */
};
#if IS_ENABLED(CONFIG_MTK_QOS_LEGACY)
static inline int qos_init_rec_share(void)
{
	return 0;
}
static inline unsigned int qos_rec_get_hist_bw(unsigned int idx, unsigned int type)
{
	return 0;
}
static inline unsigned int qos_rec_get_hist_data_bw(unsigned int idx, unsigned int type)
{
	return 0;
}
static inline unsigned int qos_rec_get_dramc_hist_bw(unsigned int idx, unsigned int type)
{
	return 0;
}
static inline unsigned int qos_rec_get_hist_idx(void)
{
	return 0xFFFF;
}
#else
extern int qos_init_rec_share(void);
extern unsigned int qos_rec_get_hist_bw(unsigned int idx,
										unsigned int type);
extern unsigned int qos_rec_get_hist_data_bw(unsigned int idx,
										unsigned int type);
extern unsigned int qos_rec_get_dramc_hist_bw(unsigned int idx,
										unsigned int type);
extern unsigned int qos_rec_get_hist_idx(void);
#endif
#define QOS_SHARE_REC_VER               0x0
#define QOS_SHARE_CURR_IDX              0x20
#define QOS_SHARE_HIST_BW               0x24
#define QOS_SHARE_HIST_DATA_BW          0xA4

extern unsigned int is_enable_qos_ltr_buffer;
#if IS_ENABLED(CONFIG_MTK_QOS_LEGACY)
static inline unsigned int qos_rec_check_sram_ext(void)
{
	return 0xFFFF;
}
static inline unsigned int qos_ltr_buffer_support(void)
{
	return 0;
}
#else
extern int qos_share_init_sram(void __iomem *regs, unsigned int bound);
extern int qos_share_init_sram_ext(void __iomem *regs, unsigned int bound);
extern int qos_share_init_sram_dbg(void __iomem *regs, unsigned int bound);
extern unsigned int qos_rec_check_sram_ext(void);
extern unsigned int qos_ltr_buffer_support(void);
extern u32 qos_share_sram_read(u32 id);
extern u32 qos_share_sram_read_ext(u32 offset);
extern u32 qos_share_sram_read_dbg(u32 offset);
#endif /* CONFIG_MTK_QOS_LEGACY */

#endif
