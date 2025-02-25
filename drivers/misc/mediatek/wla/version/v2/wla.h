/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __WLA_H__
#define __WLA_H__

#include <linux/bitfield.h>
#include <linux/types.h>
#include "memsys_wlapm_reg.h"

#define WLA_SUCCESS			(0)
#define WLA_FAIL			(-1)

#define reg_write_field(addr, val, msk)		writel((readl(addr) & (~(msk))) | (FIELD_PREP(msk, val)),addr)
#define reg_write_raw(addr, val, msk)		writel((readl(addr) & (~(msk))) | ((val) & (msk)),addr)
#define reg_read_field(addr, msk)			FIELD_GET(msk, readl(addr))
#define reg_read_raw(addr, msk)				(readl(addr) & (msk))

#define wla_write_field(offset, val, msk) \
	do {	\
		reg_write_field(wla_base+offset, val, msk); \
	} while (0)
#define wla_write_raw(offset, val, msk) \
	do {	\
		reg_write_raw(wla_base+offset, val, msk); \
	} while (0)

#define wla_read_field(offset, msk)		(reg_read_field(wla_base+offset, msk))
#define wla_read_raw(offset, msk)		(reg_read_raw(wla_base+offset, msk))

#define WLA_MON_CH_BUF		(16)
#define WLA_MON_STATUS_BUF	(6)
#define WLA_DDR_STA_MUX_BUF	(6)
#define WLA_PT_STA_MUX_BUF	(3)
#define WLA_VLC_MUX_BUF	(64)
#define WLA_TFC_MUX_BUF	(32)
#define WLA_PMSR_OCLA_MUX_BUF	(64)

#define WLA_RGLT_LBC_SHU_NUM	(2)
#define WLA_RGLT_LBC_SHU_LVL_NUM	(8)

struct wla_mon_mux_sel {
	unsigned int sig_sel;
	unsigned int bit_sel;
};

struct wla_mon_ch_setting {
	struct wla_mon_mux_sel mux;
	unsigned int trig_type;
};

struct wla_monitor {
	struct wla_mon_ch_setting ch[WLA_MON_CH_BUF];
	struct wla_mon_mux_sel pmsr_ocla_mux[WLA_PMSR_OCLA_MUX_BUF];
	struct wla_mon_mux_sel vlc_mux[WLA_VLC_MUX_BUF];
	struct wla_mon_mux_sel tfc_mux[WLA_TFC_MUX_BUF];
	/* status[0]: reserved
	 * status[1]: status1, status[2]: status2, status[3]: status3
	 */
	unsigned int ddr_sta_mux[WLA_DDR_STA_MUX_BUF];
	unsigned int pt_sta_mux[WLA_PT_STA_MUX_BUF];
	unsigned int ch_num;
	unsigned int ch_hw_max;
	unsigned int pmsr_ocla_mux_num;
	unsigned int pmsr_ocla_mux_hw_max;
	unsigned int ddr_sta_mux_num;
	unsigned int ddr_sta_mux_hw_max;
	unsigned int pt_sta_mux_num;
	unsigned int pt_sta_mux_hw_max;
	unsigned int vlc_mux_num;
	unsigned int vlc_mux_hw_max;
	unsigned int tfc_mux_num;
	unsigned int tfc_mux_hw_max;
};

struct wla_lbc_lvl_arr {
	unsigned int lvl[WLA_RGLT_LBC_SHU_LVL_NUM];
};

/* For E1/E2/... chip discrimination */
struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};

extern void __iomem *wla_base;

void wla_set_ddren_bypass(unsigned int bypass);
unsigned int wla_get_ddren_bypass(void);
void wla_set_rglt2p0_bypass(unsigned int bypass);
unsigned int wla_get_rglt2p0_bypass(void);
void wla_set_ddren_force_on(unsigned int force_on);
int wla_get_ddren_force_on(unsigned int *force_on);
unsigned int wla_get_group_num(void);
void wla_set_group_mst(unsigned int group, uint64_t mst_sel);
int wla_get_group_mst(unsigned int group, uint64_t *mst);
void wla_set_group_debounce(unsigned int group, unsigned int debounce);
int wla_get_group_debounce(unsigned int group, unsigned int *debounce);
void wla_set_group_strategy(unsigned int group, unsigned int strategy);
int wla_get_group_strategy(unsigned int group, unsigned int *strategy);
void wla_set_group_ignore_urg(unsigned int group, unsigned int ignore_urgent);
int wla_get_group_ignore_urg(unsigned int group, unsigned int *ignore_urgent);
void wla_set_dbg_latch_sel(unsigned int lat, unsigned int sel);
int wla_get_dbg_latch_sel(unsigned int lat, unsigned int *sel);
unsigned int wla_get_dbg_latch_hw_max(void);
void wla_set_dbg_lat_ddr_sta_mux(unsigned int stat_n, unsigned int mux);
int wla_get_dbg_lat_ddr_sta_mux(unsigned int stat_n, unsigned int *mux);
unsigned int wla_get_dbg_lat_ddr_sta_mux_hw_max(void);
void wla_set_lbc_shuclr_cnt(unsigned int shuclr_cnt);
unsigned int wla_get_lbc_shuclr_cnt(void);
void wla_set_lbc_shuetr_thres(unsigned int thres_cnt);
unsigned int wla_get_lbc_shuetr_thres(void);
void wla_set_lbc_shuext_thres(unsigned int thres_cnt);
unsigned int wla_get_lbc_shuext_thres(void);
void wla_set_lbc_urgent_mask(uint64_t mask);
uint64_t wla_get_lbc_urgent_mask(void);
void wla_set_lbc_lb_lvl(struct wla_lbc_lvl_arr *lvl, unsigned int shu);
int wla_get_lbc_lb_lvl(struct wla_lbc_lvl_arr *lvl, unsigned int shu);
void wla_set_plc_en(unsigned int enable);
unsigned int wla_get_plc_en(void);
void wla_set_plc_dis_trg_val(unsigned int val);
unsigned int wla_get_plc_dis_trg_val(void);
void wla_set_plc_hit_miss_thres(unsigned int thres);
unsigned int wla_get_plc_hit_miss_thres(void);
void wla_set_tfc_highbound(unsigned int highbound);
unsigned int wla_get_tfc_highbound(void);
void wla_set_tfc_ylw_timeout(unsigned int timeout);
unsigned int wla_get_tfc_ylw_timeout(void);
void wla_set_tfc_req_mask(uint64_t mask);
uint64_t wla_get_tfc_req_mask(void);
void wla_set_tfc_urg_mask(uint64_t mask);
uint64_t wla_get_tfc_urg_mask(void);

void wla_mon_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set);
int wla_mon_get_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set);
void wla_mon_set_ddr_sta_mux(unsigned int stat_n, unsigned int mux);
int wla_mon_get_ddr_sta_mux(unsigned int stat_n, unsigned int *mux);
void wla_mon_set_pt_sta_mux(unsigned int stat_n, unsigned int mux);
int wla_mon_get_pt_sta_mux(unsigned int stat_n, unsigned int *mux);
void wla_mon_set_vlc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux);
int wla_mon_get_vlc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux);
void wla_mon_set_tfc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux);
int wla_mon_get_tfc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux);
void wla_mon_set_pmsr_ocla_mux(unsigned int mux_num,
							struct wla_mon_mux_sel *mux);
int wla_mon_get_pmsr_ocla_mux(unsigned int mux_num,
							struct wla_mon_mux_sel *mux);
int wla_mon_start(unsigned int win_len_sec);
void wla_mon_disable(void);
unsigned int wla_mon_get_ch_hw_max(void);
unsigned int wla_mon_get_ddr_sta_mux_hw_max(void);
unsigned int wla_mon_get_pt_sta_mux_hw_max(void);
unsigned int wla_mon_get_vlc_mux_hw_max(void);
unsigned int wla_mon_get_tfc_mux_hw_max(void);
unsigned int wla_mon_get_pmsr_ocla_mux_hw_max(void);
int wla_mon_get_ch_cnt(unsigned int ch, uint32_t *north_cnt,
							uint32_t *south_cnt);
int wla_mon_init(struct wla_monitor *mon);

#endif	// __WLA_H__
