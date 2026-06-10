// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include "wla.h"

#define WLA__INIT_SETTING_VER		"init-setting-ver"
#define WLA__ENABLE					"wla-enable"
#define WLA__DDREN_FORCE_ON			"ddren-force-on"
#define WLA__GROUP_HW_MAX			"group-hw-max"
#define WLA__GROUP_NODE				"group-ctrl"
#define WLA__GROUP_MASTER_SEL		"master-sel"
#define WLA__GROUP_DEBOUNCE			"debounce"
#define WLA__GROUP_STRATEGY			"strategy"
#define WLA__GROUP_IGNORE_URGENT	"ignore-urgent"
#define WLA__MON_CH_NODE			"mon-ch"
#define WLA__MON_CH_HW_MAX			"mon-ch-hw-max"
#define WLA__MON_SIG_SEL			"sig-sel"
#define WLA__MON_BIT_SEL			"bit-sel"
#define WLA__MON_TRIG_TYPE			"trig-type"
#define WLA__MON_DDR_STA_MUX_NODE	"mon-ddr-sta-mux"
#define WLA__MON_DDR_STA_MUX_HW_MAX	"mon-ddr-sta-mux-hw-max"
#define WLA__MON_PT_STA_MUX_NODE	"mon-pt-sta-mux"
#define WLA__MON_PT_STA_MUX_HW_MAX	"mon-pt-sta-mux-hw-max"
#define WLA__STA_MUX				"mux"
#define WLA__DBG_LAT_HW_MAX			"dbg-lat-hw-max"
#define WLA__DBG_LAT_NODE			"dbg-lat"
#define WLA__DBG_LATCH_SEL			"sel"
#define WLA__DBG_LAT_DDR_STA_MUX_NODE	"dbg-lat-ddr-sta-mux"
#define WLA__DBG_LAT_DDR_STA_MUX_HW_MAX	"dbg-lat-ddr-sta-mux-hw-max"
#define WLA__LBC_SHUCLR_CNT			"lbc-shuclr-cnt"
#define WLA__LBC_SHUETR_THRES		"lbc-shuetr-thres"
#define WLA__LBC_SHUEXT_THRES		"lbc-shuext-thres"
#define WLA__LBC_MST_URG_MSK		"lbc-mst-urg-msk"
#define WLA__LBC_SHU_LB_LVL			"lbc-shu-lb-lvl"
#define WLA__PLC_EN					"plc-en"
#define WLA__PLC_DIS_TARGET_VAL		"plc-dis-target-val"
#define WLA__PLC_HIT_MISS_THRES		"plc-hit-miss-thres"
#define WLA__TFC_HIGHBOUND			"tfc-highbound"
#define WLA__TFC_YLW_TIMEOUT		"tfc-ylw-timeout"
#define WLA__TFC_MST_REQ_MSK		"tfc-mst-req-msk"
#define WLA__TFC_MST_URG_MSK		"tfc-mst-urg-msk"
#define WLA__VLC_MUX_HW_MAX			"vlc-mux-hw-max"
#define WLA__TFC_MUX_HW_MAX			"tfc-mux-hw-max"
#define WLA__PMSR_OCLA_MUX_HW_MAX	"pmsr-ocla-mux-hw-max"
#define WLA__VLC_MUX_NODE			"vlc-mux"
#define WLA__TFC_MUX_NODE			"tfc-mux"
#define WLA__PMSR_OCLA_MUX_NODE		"pmsr-ocla-mux"
#define WLA__BUS_PROT_INST_LIST		"bus-prot-inst-list"

#define WLA_GROUP_BUF	(10)
#define WLA_DBG_LATCH_BUF	(10)
#define WLA_DBG_LAT_DDR_STA_MUX_BUF	(6)
#define WLA_BUS_PROT_BUF	(96)

void __iomem *wla_base;

enum wla_default_mode {
	DEFAULT_BYPASS_MODE = 0,
	DEFAULT_STANDBY_MODE,
	DEFAULT_2P0_MODE
};

enum wla_grp_strategy {
	IDLE_0 = 0,
	WAKEUP_BY_SLC_DDREN_REQ,
	WAKEUP_BY_SLC_IDLE,
	WAKEUP_BY_EMI_CHN_IDLE,
	IDLE_4,
	IDLE_5,
	PST_REQ,
	IDLE_7
};

struct wla_group_info {
	uint64_t master_sel;
	unsigned int debounce;
	unsigned int strategy;
	unsigned int ignore_urg;
};

struct wla_group {
	struct wla_group_info info[WLA_GROUP_BUF];
	unsigned int num;
	unsigned int hw_max;
};

struct wla_ddren_ctrl {
	struct wla_group grp;
	unsigned int ddren_force_on;
};

struct wla_dbg_latch {
	unsigned int lat_sel[WLA_DBG_LATCH_BUF];
	unsigned int lat_num;
	unsigned int lat_hw_max;
	unsigned int ddr_sta_mux[WLA_DBG_LAT_DDR_STA_MUX_BUF];
	unsigned int ddr_sta_mux_num;
	unsigned int ddr_sta_mux_hw_max;
};

struct wla_rglt2p0_lbc {
	/* detect window, unit: cnt/26M s */
	unsigned int shuclr_cnt;
	/* urgent cnt to enter shu1, unit: cnt */
	unsigned int shuetr_thres;
	/* resume to shu0 cnt, unit: red/green loop */
	unsigned int shuext_thres;
	uint64_t urg_msk;
	struct wla_lbc_lvl_arr shu[WLA_RGLT_LBC_SHU_NUM];
};

struct wla_rglt2p0_plc {
	unsigned int enable;
	unsigned int dis_trg_val;
	unsigned int hit_miss_thres;
};

struct wla_rglt2p0_tfc {
	/* unit: /26M s */
	unsigned int highbound;
	unsigned int ylw_timeout;
	uint64_t req_msk;
	uint64_t urg_msk;
};

struct wla_rglt2p0 {
	struct wla_rglt2p0_lbc lbc;
	struct wla_rglt2p0_plc plc;
	struct wla_rglt2p0_tfc tfc;
};

struct wla_bus_prot_inst {
	unsigned int buf[WLA_BUS_PROT_BUF];
	unsigned int len;
};

static struct wla_ddren_ctrl wla_ddr_ctrl;
static struct wla_monitor wla_mon;
static struct wla_dbg_latch wla_lat;
static struct wla_rglt2p0 rglt2p0;
static enum wla_default_mode default_mode = DEFAULT_BYPASS_MODE;

void wla_set_ddren_bypass(unsigned int bypass_mode)
{
	unsigned int runtime_switch_en;

	if (bypass_mode > 1)
		return;

	if (default_mode == DEFAULT_BYPASS_MODE)
		bypass_mode = 1;

	runtime_switch_en = !bypass_mode;
	/* notify spmfw to switch to bypass mode or not */
	wla_write_field(WLAPM_CLK_CTRL0, runtime_switch_en, BIT(0));
	wla_write_field(WLAPM_DDREN_CTRL0, bypass_mode,
						WLAPM_DDREN_BYPASS_FSM_CTRL);
}
EXPORT_SYMBOL(wla_set_ddren_bypass);

unsigned int wla_get_ddren_bypass(void)
{
	return wla_read_field(WLAPM_DDREN_CTRL0, WLAPM_DDREN_BYPASS_FSM_CTRL);
}
EXPORT_SYMBOL(wla_get_ddren_bypass);

void wla_set_rglt2p0_bypass(unsigned int bypass_mode)
{
	unsigned int runtime_switch_en;

	if (bypass_mode > 1)
		return;

	if (default_mode != DEFAULT_2P0_MODE)
		bypass_mode = 1;

	runtime_switch_en = !bypass_mode;
	/* notify spmfw to switch to bypass mode or not */
	wla_write_field(WLAPM_CLK_CTRL0, runtime_switch_en, BIT(1));
	wla_write_field(WLAPM_2P0_RGU4, bypass_mode, WLAPM_2P0_TFC_BYPASS_MODE);
}
EXPORT_SYMBOL(wla_set_rglt2p0_bypass);

unsigned int wla_get_rglt2p0_bypass(void)
{
	return wla_read_field(WLAPM_2P0_RGU4, WLAPM_2P0_TFC_BYPASS_MODE);
}
EXPORT_SYMBOL(wla_get_rglt2p0_bypass);

void wla_set_ddren_force_on(unsigned int force_on)
{
	unsigned int runtime_switch_en = !force_on;

	/* workaround: notify sspm slc driver to switch to force_on or not */
	wla_write_field(WLAPM_CLK_CTRL0, runtime_switch_en, BIT(2));
	wla_write_field(WLAPM_DDREN_CTRL0, force_on, WLAPM_DDREN_FORCE_ON);
}
EXPORT_SYMBOL(wla_set_ddren_force_on);

int wla_get_ddren_force_on(unsigned int *force_on)
{
	*force_on = wla_read_field(WLAPM_DDREN_CTRL0, WLAPM_DDREN_FORCE_ON);

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_ddren_force_on);

unsigned int wla_get_group_num(void)
{
	return wla_ddr_ctrl.grp.num;
}
EXPORT_SYMBOL(wla_get_group_num);

void wla_set_group_mst(unsigned int group, uint64_t mst_sel)
{
	if (group >= wla_ddr_ctrl.grp.hw_max)
		return;

	wla_write_field(WLAPM_RGU_GRP0 + group * 8,
						(uint32_t)(mst_sel & 0xffffffff), GENMASK(31, 0));
	wla_write_field(WLAPM_RGU_GRP1 + group * 8,
						(uint32_t)(mst_sel >> 32), GENMASK(31, 0));
}
EXPORT_SYMBOL(wla_set_group_mst);

int wla_get_group_mst(unsigned int group, uint64_t *mst)
{
	uint64_t value;

	if (group >= wla_ddr_ctrl.grp.hw_max || mst == NULL)
		return WLA_FAIL;
	/* get grp mst */
	value = wla_read_field(WLAPM_RGU_GRP1 + group * 8, GENMASK(31, 0));
	value = (value << 32) |
					wla_read_field(WLAPM_RGU_GRP0 + group * 8,
											GENMASK(31, 0));
	*mst = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_mst);

void wla_set_group_debounce(unsigned int group, unsigned int debounce)
{
	if (group >= wla_ddr_ctrl.grp.hw_max)
		return;

	wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, debounce,
						WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);
}
EXPORT_SYMBOL(wla_set_group_debounce);

int wla_get_group_debounce(unsigned int group, unsigned int *debounce)
{
	unsigned int value;

	if (group >= wla_ddr_ctrl.grp.hw_max || debounce == NULL)
		return WLA_FAIL;

	value =
		wla_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
								WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);

	*debounce = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_debounce);

void wla_set_group_strategy(unsigned int group, unsigned int strategy)
{
	if (group >= wla_ddr_ctrl.grp.hw_max)
		return;

	wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, strategy,
						WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
	wla_write_field(WLAPM_DDREN_GRP_CTRL0 + group*4, 0x1,
						WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_ENABLE);

	if (strategy == PST_REQ)
		wla_write_raw(WLAPM_2P0_DDREN_CTRL7, 0x1 << group, 0x1 << group);
	else
		wla_write_raw(WLAPM_2P0_DDREN_CTRL7, 0x0, 0x1 << group);
}
EXPORT_SYMBOL(wla_set_group_strategy);

int wla_get_group_strategy(unsigned int group, unsigned int *strategy)
{
	unsigned int value;

	if (group >= wla_ddr_ctrl.grp.hw_max || strategy == NULL)
		return WLA_FAIL;

	value = wla_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
	*strategy = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_strategy);

void wla_set_group_ignore_urg(unsigned int group, unsigned int ignore_urg)
{
	if (group >= wla_ddr_ctrl.grp.hw_max)
		return;

	wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), ignore_urg,
						WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
}
EXPORT_SYMBOL(wla_set_group_ignore_urg);

int wla_get_group_ignore_urg(unsigned int group, unsigned int *ignore_urg)
{
	unsigned int value;

	if (group >= wla_ddr_ctrl.grp.hw_max || ignore_urg == NULL)
		return WLA_FAIL;

	value = wla_read_field(WLAPM_DDREN_GRP_CTRL0 + group*4,
										WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
	*ignore_urg = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_group_ignore_urg);

void wla_set_dbg_latch_sel(unsigned int lat, unsigned int sel)
{
	if (lat >= wla_lat.lat_hw_max)
		return;

	wla_write_raw(WLAPM_DBG_CTRL0, sel << (lat * 3), 0x7U << (lat * 3));
}
EXPORT_SYMBOL(wla_set_dbg_latch_sel);

int wla_get_dbg_latch_sel(unsigned int lat, unsigned int *sel)
{
	unsigned int value;

	if (lat >= wla_lat.lat_hw_max || sel == NULL)
		return WLA_FAIL;
	/* get dbg latch sel */
	value = wla_read_raw(WLAPM_DBG_CTRL0, 0x7U << (lat * 3));
	value = value >> (lat * 3);
	*sel = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_dbg_latch_sel);

unsigned int wla_get_dbg_latch_hw_max(void)
{
	return wla_lat.lat_hw_max;
}
EXPORT_SYMBOL(wla_get_dbg_latch_hw_max);

void wla_set_dbg_lat_ddr_sta_mux(unsigned int stat_n, unsigned int mux)
{
	if (stat_n > wla_lat.ddr_sta_mux_hw_max || stat_n < 1)
		return;

	wla_write_raw(WLAPM_DDREN_DBG, mux << ((stat_n - 1) * 4),
					0xFU << ((stat_n - 1) * 4));
}
EXPORT_SYMBOL(wla_set_dbg_lat_ddr_sta_mux);

int wla_get_dbg_lat_ddr_sta_mux(unsigned int stat_n, unsigned int *mux)
{
	unsigned int value;

	if (stat_n < 1 || stat_n > wla_lat.lat_hw_max)
		return WLA_FAIL;
	/* get dbg latch sta mux */
	value = wla_read_raw(WLAPM_DDREN_DBG, 0xFU << ((stat_n - 1) * 4));
	value = value >> ((stat_n - 1) * 4);
	*mux = value;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_dbg_lat_ddr_sta_mux);

unsigned int wla_get_dbg_lat_ddr_sta_mux_hw_max(void)
{
	return wla_lat.ddr_sta_mux_hw_max;
}
EXPORT_SYMBOL(wla_get_dbg_lat_ddr_sta_mux_hw_max);

void wla_set_lbc_shuclr_cnt(unsigned int shuclr_cnt)
{
	wla_write_field(WLAPM_2P0_RGU12, shuclr_cnt, WLAPM_2P0_LBC_SHUCLR_CNT);
}
EXPORT_SYMBOL(wla_set_lbc_shuclr_cnt);

unsigned int wla_get_lbc_shuclr_cnt(void)
{
	return wla_read_field(WLAPM_2P0_RGU12, WLAPM_2P0_LBC_SHUCLR_CNT);
}
EXPORT_SYMBOL(wla_get_lbc_shuclr_cnt);

void wla_set_lbc_shuetr_thres(unsigned int thres_cnt)
{
	wla_write_field(WLAPM_2P0_RGU12, thres_cnt, WLAPM_2P0_LBC_SHUETR_CNT);
}
EXPORT_SYMBOL(wla_set_lbc_shuetr_thres);

unsigned int wla_get_lbc_shuetr_thres(void)
{
	return wla_read_field(WLAPM_2P0_RGU12, WLAPM_2P0_LBC_SHUETR_CNT);
}
EXPORT_SYMBOL(wla_get_lbc_shuetr_thres);

void wla_set_lbc_shuext_thres(unsigned int thres_cnt)
{
	wla_write_field(WLAPM_2P0_RGU12, thres_cnt, WLAPM_2P0_LBC_SHUEXT_CNT);
}
EXPORT_SYMBOL(wla_set_lbc_shuext_thres);

unsigned int wla_get_lbc_shuext_thres(void)
{
	return wla_read_field(WLAPM_2P0_RGU12, WLAPM_2P0_LBC_SHUEXT_CNT);
}
EXPORT_SYMBOL(wla_get_lbc_shuext_thres);

void wla_set_lbc_urgent_mask(uint64_t mask)
{
	wla_write_field(WLAPM_2P0_RGU34,
						(uint32_t)(mask & 0xffffffff),
						WLAPM_2P0_LBC_MST_URG_MSK0);
	wla_write_field(WLAPM_2P0_RGU35,
						(uint32_t)(mask >> 32), WLAPM_2P0_LBC_MST_URG_MSK1);
}
EXPORT_SYMBOL(wla_set_lbc_urgent_mask);

uint64_t wla_get_lbc_urgent_mask(void)
{
	uint64_t value;

	value = wla_read_field(WLAPM_2P0_RGU35, WLAPM_2P0_LBC_MST_URG_MSK1);
	value = (value << 32) | wla_read_field(WLAPM_2P0_RGU34,
											WLAPM_2P0_LBC_MST_URG_MSK0);

	return value;
}
EXPORT_SYMBOL(wla_get_lbc_urgent_mask);

void wla_set_lbc_lb_lvl(struct wla_lbc_lvl_arr *lvl, unsigned int shu)
{
	unsigned int i;

	if (!lvl || shu >= WLA_RGLT_LBC_SHU_NUM)
		return;

	for (i = 0; i < (WLA_RGLT_LBC_SHU_LVL_NUM / 2); i++) {
		wla_write_field(WLAPM_2P0_RGU13 + (i * 0x4) + (shu * 0x10),
						lvl->lvl[i * 2], WLAPM_2P0_LBC_SHU0_LB_LV0);
		wla_write_field(WLAPM_2P0_RGU13 + (i * 0x4) + (shu * 0x10),
						lvl->lvl[i * 2 + 1], WLAPM_2P0_LBC_SHU0_LB_LV1);
	}
}
EXPORT_SYMBOL(wla_set_lbc_lb_lvl);

int wla_get_lbc_lb_lvl(struct wla_lbc_lvl_arr *lvl, unsigned int shu)
{
	unsigned int i;

	if (!lvl || shu >= WLA_RGLT_LBC_SHU_NUM)
		return WLA_FAIL;

	for (i = 0; i < (WLA_RGLT_LBC_SHU_LVL_NUM / 2); i++) {
		lvl->lvl[i * 2] = wla_read_field(WLAPM_2P0_RGU13 +
						(i * 0x4) + (shu * 0x10), WLAPM_2P0_LBC_SHU0_LB_LV0);
		lvl->lvl[i * 2 + 1] = wla_read_field(WLAPM_2P0_RGU13 +
						(i * 0x4) + (shu * 0x10), WLAPM_2P0_LBC_SHU0_LB_LV1);
	}

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_get_lbc_lb_lvl);

void wla_set_plc_en(unsigned int enable)
{
	wla_write_field(WLAPM_2P0_CKCTRL0, enable, WLAPM_2P0_PLC_EN);
}
EXPORT_SYMBOL(wla_set_plc_en);

unsigned int wla_get_plc_en(void)
{
	return wla_read_field(WLAPM_2P0_CKCTRL0, WLAPM_2P0_PLC_EN);
}
EXPORT_SYMBOL(wla_get_plc_en);

void wla_set_plc_dis_trg_val(unsigned int val)
{
	/* undefined value will be ignored */
	if (val != 0x8 && val != 0xc && val != 0xe)
		return;
	/* lv1 = 0b1110 lv2 = 0b1100 lv3 = 0b1000 */
	wla_write_field(WLAPM_2P0_RGU33, val, WLAPM_2P0_PLC_TARGET_DIS);
}
EXPORT_SYMBOL(wla_set_plc_dis_trg_val);

unsigned int wla_get_plc_dis_trg_val(void)
{
	return wla_read_field(WLAPM_2P0_RGU33, WLAPM_2P0_PLC_TARGET_DIS);
}
EXPORT_SYMBOL(wla_get_plc_dis_trg_val);

void wla_set_plc_hit_miss_thres(unsigned int thres)
{
	wla_write_field(WLAPM_2P0_RGU31, thres, WLAPM_2P0_PLC_PSTLEV_MANUAL);
}
EXPORT_SYMBOL(wla_set_plc_hit_miss_thres);

unsigned int wla_get_plc_hit_miss_thres(void)
{
	return wla_read_field(WLAPM_2P0_RGU31, WLAPM_2P0_PLC_PSTLEV_MANUAL);
}
EXPORT_SYMBOL(wla_get_plc_hit_miss_thres);

void wla_set_tfc_highbound(unsigned int highbound)
{
	wla_write_field(WLAPM_2P0_RGU0, highbound, WLAPM_2P0_TFC_GRN_HIGHBOUND);
}
EXPORT_SYMBOL(wla_set_tfc_highbound);

unsigned int wla_get_tfc_highbound(void)
{
	return wla_read_field(WLAPM_2P0_RGU0, WLAPM_2P0_TFC_GRN_HIGHBOUND);
}
EXPORT_SYMBOL(wla_get_tfc_highbound);

void wla_set_tfc_ylw_timeout(unsigned int timeout)
{
	wla_write_field(WLAPM_2P0_RGU1, timeout, WLAPM_2P0_TFC_YLW_HIGHBOUND);
}
EXPORT_SYMBOL(wla_set_tfc_ylw_timeout);

unsigned int wla_get_tfc_ylw_timeout(void)
{
	return wla_read_field(WLAPM_2P0_RGU1, WLAPM_2P0_TFC_YLW_HIGHBOUND);
}
EXPORT_SYMBOL(wla_get_tfc_ylw_timeout);

void wla_set_tfc_req_mask(uint64_t mask)
{
	wla_write_field(WLAPM_2P0_RGU5,
						(uint32_t)(mask & 0xffffffff),
						WLAPM_2P0_TFC_MST_REQ_MSK0);
	wla_write_field(WLAPM_2P0_RGU6,
						(uint32_t)(mask >> 32), WLAPM_2P0_TFC_MST_REQ_MSK1);
}
EXPORT_SYMBOL(wla_set_tfc_req_mask);

uint64_t wla_get_tfc_req_mask(void)
{
	uint64_t value;

	value = wla_read_field(WLAPM_2P0_RGU6, WLAPM_2P0_TFC_MST_REQ_MSK1);
	value = (value << 32) | wla_read_field(WLAPM_2P0_RGU5,
											WLAPM_2P0_TFC_MST_REQ_MSK0);

	return value;
}
EXPORT_SYMBOL(wla_get_tfc_req_mask);

void wla_set_tfc_urg_mask(uint64_t mask)
{
	wla_write_field(WLAPM_2P0_RGU9,
						(uint32_t)(mask & 0xffffffff),
						WLAPM_2P0_TFC_MST_URG_MSK0);
	wla_write_field(WLAPM_2P0_RGU10,
						(uint32_t)(mask >> 32), WLAPM_2P0_TFC_MST_URG_MSK1);
}
EXPORT_SYMBOL(wla_set_tfc_urg_mask);

uint64_t wla_get_tfc_urg_mask(void)
{
	uint64_t value;

	value = wla_read_field(WLAPM_2P0_RGU10, WLAPM_2P0_TFC_MST_URG_MSK1);
	value = (value << 32) | wla_read_field(WLAPM_2P0_RGU9,
											WLAPM_2P0_TFC_MST_URG_MSK0);

	return value;
}
EXPORT_SYMBOL(wla_get_tfc_urg_mask);

static int wla_dbg_latch_init(struct wla_dbg_latch *lat)
{
	unsigned int n;

	if (lat == NULL)
		return WLA_FAIL;

	for (n = 0; n < lat->lat_num; n++)
		wla_set_dbg_latch_sel(n, lat->lat_sel[n]);

	for (n = 1; n <= lat->ddr_sta_mux_num; n++)
		wla_set_dbg_lat_ddr_sta_mux(n, lat->ddr_sta_mux[n]);

	return WLA_SUCCESS;
}

static int get_chipid(void)
{
	struct device_node *node;
	struct tag_chipid *chip_id;
	int len;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");

	if (node) {
		chip_id = (struct tag_chipid *)of_get_property(node,
								"atag,chipid",
								&len);
		if (!chip_id) {
			pr_info("could not found atag,chipid in chosen\n");
			return -ENODEV;
		}
	} else {
		pr_info("chosen node not found in device tree\n");
		return -ENODEV;
	}

	pr_info("[mtk-wla] current sw version: %u\n", chip_id->sw_ver);

	return chip_id->sw_ver;
}

static void __wla_bypass_mode_init(void)
{
	wla_write_field(WLAPM_CLK_CTRL1, 0x1, WLAPM_CKCTRL_DBG_CK_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FROM_LEGACY_MODE);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_HWS1_FSM_IDLE_DBG_MON_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER_EN);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_DDREN_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_HWS1_FSM_COUNTER);
}

static void __wla_standby_power_init(void)
{
	unsigned int group;

	wla_write_field(WLAPM_CLK_CTRL1, 0x1, WLAPM_CKCTRL_DBG_CK_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0, WLAPM_DDREN_FROM_LEGACY_MODE);

	wla_write_field(WLAPM_DDREN_CTRL0, 0x1,
						WLAPM_DDREN_DDREN_FSM_IGNORE_ACK_WINDOW_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_HWS1_FSM_IDLE_DBG_MON_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER_EN);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x12,
						WLAPM_DDREN_FSM_IGNORE_ACK_WINDOW_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0x0,
						WLAPM_DDREN_FSM_BYPASS_SLC_REQ);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_DDREN_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_HWS1_FSM_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0xf,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_CHN_EMI_IDLE);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0xf,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_SLC_IDLE);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0x0,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_SLC_DDREN_REQ);


	wla_write_field(WLAPM_DDREN_CTRL2, 0x3,
						WLAPM_HWS1_HIGH_MONITOR_ACCESS_DRAM_BUSY_EN);
	wla_write_field(WLAPM_DDREN_CTRL2, 0x100,
						WLAPM_HWS1_HIGH_MONITOR_ACCESS_DRAM_BUSY_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0x0, WLAPM_CHN_EMI_IDLE_POR_ADJ);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0, WLAPM_CHN_EMI_IDLE_BYPASS);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0, WLAPM_SLC_IDLE_PRO_ADJ);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0, WLAPM_SLC_IDLE_EN_BYPASS);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_HWS1_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_DDREN_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x1a, WLAPM_HWS1_FSM_DBG_IDLE_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0, WLAPM_SLC_READ_MISS_DDREN_BYPASS);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0, WLAPM_SLC_READ_MISS_DDREN_POR_ADJ);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0,
						WLAPM_SLC_WRITE_THROUGH_DDREN_BYPASS);
	wla_write_field(WLAPM_2P0_DDREN_CTRL5, 0x0,
						WLAPM_SLC_WRITE_THROUGH_DDREN_POR_ADJ);

	for (group = 0; group < wla_ddr_ctrl.grp.hw_max; group++) {
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0xd,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_COUNT);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_STANDY_STRATEGY_SEL);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_STANDY_DRAM_IDLE_ENABLE);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_IGNORE_URGENT_EN);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_WAKEUP_MODE_SEL);
		wla_write_field(WLAPM_DDREN_GRP_CTRL0 + (4*group), 0x0,
							WLAPM_DDREN_GRP0_ACK_MODE_SEL);
	}
}

static void __wla_2p0_init(struct wla_rglt2p0 *rglt, struct wla_bus_prot_inst *bus_prot)
{
	unsigned int i;

	/* rglt2p0 bypass mode setting */
	wla_write_field(WLAPM_2P0_RGU4, 1, WLAPM_2P0_TFC_BYPASS_MODE);
	wla_write_field(WLAPM_2P0_PTCTRL0, 1, WLAPM_2P0_PTCTRL_PROG_EN);
	wla_write_field(WLAPM_2P0_RGU4, 1, WLAPM_2P0_TFC_BYPASS_MODE_UPD);
	wla_write_field(WLAPM_2P0_RGU4, 0, WLAPM_2P0_TFC_PROT_BYPASS);
	wla_write_field(WLAPM_2P0_RGU4, 0, WLAPM_2P0_TFC_PST_BYPASS);

	/* DDREN CTRL */
	wla_write_field(WLAPM_CLK_CTRL1, 1, WLAPM_CKCTRL_DBG_CK_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_HWS1_FSM_IDLE_DBG_MON_EN);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x0,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER_EN);
	wla_write_field(WLAPM_DDREN_CTRL4, 0x12,
						WLAPM_DDREN_FSM_IGNORE_ACK_WINDOW_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0xf,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_CHN_EMI_IDLE);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0xf,
						WLAPM_DDREN_FSM_ACCESS_BUSY_BYPASS_SLC_IDLE);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x1a,
						WLAPM_DDREN_NON_IDLE_WAKEUP_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL0, 0x1, WLAPM_DDREN_SYNC_IN_ENABLE);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x34,
						WLAPM_DDREN_DDREN_FSM_FIX_WAKEUP_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0xd,
						WLAPM_DDREN_DDREN_FSM_IDLE_CHK_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_HWS1_FSM_COUNTER);
	wla_write_field(WLAPM_DDREN_CTRL1, 0x0,
						WLAPM_DDREN_DDREN_FSM_COUNTER);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0x0,
						WLAPM_HWS1_FSM_BYPASS_ACCESS_DRAM_BUSY);
	wla_write_field(WLAPM_2P0_DDREN_CTRL6, 0x0,
						WLAPM_DDREN_FSM_BYPASS_SLC_REQ);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_HWS1_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x69, WLAPM_DDREN_FSM_B2B_S1_CTRL);
	wla_write_field(WLAPM_DDREN_CTRL3, 0x1a, WLAPM_HWS1_FSM_DBG_IDLE_COUNTER);

	/* drm ddren_urg tie1 for dfd wakeup dram workaround */
	wla_write_field(WLAPM_TEST21, 0x1000, WLAPM_TEST_URG_TIE_EN_1);

	/* regulator 2p0 */
	wla_write_field(WLAPM_2P0_RGU1, 0x0, WLAPM_2P0_TFC_EGRN_CNT);
	wla_write_field(WLAPM_2P0_RGU1, 0x0, WLAPM_2P0_TFC_EGRN_CNT_URG);
	wla_set_lbc_shuclr_cnt(rglt->lbc.shuclr_cnt);
	wla_set_lbc_shuetr_thres(rglt->lbc.shuetr_thres);
	wla_set_lbc_shuext_thres(rglt->lbc.shuext_thres);
	wla_set_lbc_urgent_mask(rglt->lbc.urg_msk);
	wla_set_lbc_lb_lvl(&rglt->lbc.shu[0], 0);
	wla_set_lbc_lb_lvl(&rglt->lbc.shu[1], 1);
	wla_set_plc_en(rglt->plc.enable);
	wla_set_plc_dis_trg_val(rglt->plc.dis_trg_val);
	wla_set_plc_hit_miss_thres(rglt->plc.hit_miss_thres);
	wla_set_tfc_highbound(rglt->tfc.highbound);
	wla_set_tfc_ylw_timeout(rglt->tfc.ylw_timeout);
	wla_set_tfc_req_mask(rglt->tfc.req_msk);
	wla_set_tfc_urg_mask(rglt->tfc.urg_msk);

	/* bus protect */
	if (bus_prot->len > WLA_BUS_PROT_BUF)
		return;

	wla_write_field(WLAPM_2P0_PTCTRL97, 0x1,
						WLAPM_2P0_PTCTRL_COMP1_BYPASS_URG);
	wla_write_field(WLAPM_2P0_PTCTRL97, 0x1,
						WLAPM_2P0_PTCTRL_COMP1_BYPASS_EXCEPTION);
	wla_write_field(WLAPM_2P0_PTCTRL97, 0x1,
						WLAPM_2P0_PTCTRL_COMP1_BYPASS_TIMEOUT);
	wla_write_field(WLAPM_2P0_PTCTRL97, 0x1,
						WLAPM_2P0_PTCTRL_COMP1_JUMP_EN);
	wla_write_field(WLAPM_2P0_PTCTRL0, 0x1,
						WLAPM_2P0_PTCTRL_CHIFR_SUSPEND_MODE);
	wla_write_field(WLAPM_2P0_PTCTRL0, 0x104,
						WLAPM_2P0_PTCTRL_TIMEOUT_CNT);
	wla_write_field(WLAPM_2P0_PTCTRL0, 0x1,
						WLAPM_2P0_PTCTRL_PROG_EN);
	wla_write_field(WLAPM_2P0_RGU2, 0xd0, WLAPM_2P0_TFC_PST_PERIOD1);
	wla_write_field(WLAPM_2P0_RGU2, 0x3dc, WLAPM_2P0_TFC_PST_PERIOD2);
	wla_write_field(WLAPM_2P0_RGU3, 0x8f0, WLAPM_2P0_TFC_PST_PERIOD3);

	for (i = 0; i < bus_prot->len; i++)
		wla_write_field(WLAPM_2P0_PTCTRL1 + (i * 4),
					bus_prot->buf[i], GENMASK(31, 0));
}

static int wla_init(const char *init_ver, struct wla_ddren_ctrl *ddr_ctrl,
				struct wla_rglt2p0 *rglt, struct wla_bus_prot_inst *bus_prot)
{
	unsigned int grp;
	unsigned int ddren_bypass = 1;
	unsigned int rglt2p0_bypass = 1;
	struct wla_group *group = &ddr_ctrl->grp;

	if (!strcmp(init_ver, "wla_2p0_ver_a")) {
		__wla_2p0_init(rglt, bus_prot);
		default_mode = DEFAULT_2P0_MODE;
		ddren_bypass = 0;
		rglt2p0_bypass = 0;
	} else if (!strcmp(init_ver, "standby_power_ver_a")) {
		__wla_standby_power_init();
		default_mode = DEFAULT_STANDBY_MODE;
		ddren_bypass = 0;
	} else if (!strcmp(init_ver, "bypass_mode")) {
		__wla_bypass_mode_init();
		default_mode = DEFAULT_BYPASS_MODE;
	} else if (!strcmp(init_ver, "depend_on_chipid_ver_a")) {
		if (get_chipid()) {
			__wla_standby_power_init();
			default_mode = DEFAULT_STANDBY_MODE;
			ddren_bypass = 0;
		} else {
			__wla_bypass_mode_init();
			default_mode = DEFAULT_BYPASS_MODE;
		}
	} else
		return WLA_FAIL;

	for (grp = 0; grp < group->num; grp++) {
		/* set group master */
		wla_set_group_mst(grp, group->info[grp].master_sel);
		/* standby strategy */
		/* 0x7=idle, 0x1=s1(wake up by slc_ddren_req),
		 * 0x2=s1(wake up by slc_idle), 0x3=s1(wake up by emi_chnc_idle)
		 */
		wla_set_group_strategy(grp, group->info[grp].strategy);
		/* group debounce */
		wla_set_group_debounce(grp, group->info[grp].debounce);
		/* group ignore urgent */
		wla_set_group_ignore_urg(grp, group->info[grp].ignore_urg);
	}
	/* set grp en */
	wla_write_field(WLAPM_RGU_CTRL0, ((1U << wla_ddr_ctrl.grp.num) - 1),
						WLAPM_RGU_GRP_EN);

	wla_set_ddren_bypass(ddren_bypass);
	wla_set_rglt2p0_bypass(rglt2p0_bypass);

	/* ddren free run */
	wla_set_ddren_force_on(ddr_ctrl->ddren_force_on);

	return WLA_SUCCESS;
}

static int wla_probe(struct platform_device *pdev)
{
	struct device_node *parent_node = pdev->dev.of_node;
	struct device_node *np;
	const char *init_setting_ver;
	struct wla_ddren_ctrl *ddr_ctrl = &wla_ddr_ctrl;
	struct wla_group *group = &ddr_ctrl->grp;
	struct wla_monitor *mon = &wla_mon;
	struct wla_dbg_latch *lat = &wla_lat;
	struct wla_rglt2p0 *rglt = &rglt2p0;
	struct wla_bus_prot_inst bus_prot;
	const __be32 *buf;
	unsigned int wla_en = 0;
	unsigned int len, i, j;
	int ret = WLA_FAIL;

	wla_base = of_iomap(parent_node, 0);

	of_property_read_u32(parent_node, WLA__ENABLE, &wla_en);

	if (!wla_en) {
		pr_info("[mtk-wla] wla disable\n");
		return ret;
	}

	of_property_read_u32(parent_node, WLA__DDREN_FORCE_ON,
								&ddr_ctrl->ddren_force_on);
	of_property_read_string(parent_node, WLA__INIT_SETTING_VER,
								&init_setting_ver);
	of_property_read_u32(parent_node, WLA__GROUP_HW_MAX, &group->hw_max);
	while ((np = of_parse_phandle(parent_node, WLA__GROUP_NODE,
									group->num))) {
		of_property_read_u64(np, WLA__GROUP_MASTER_SEL,
								&group->info[group->num].master_sel);
		of_property_read_u32(np, WLA__GROUP_DEBOUNCE,
								&group->info[group->num].debounce);
		of_property_read_u32(np, WLA__GROUP_STRATEGY,
								&group->info[group->num].strategy);
		of_property_read_u32(np, WLA__GROUP_IGNORE_URGENT,
								&group->info[group->num].ignore_urg);
		of_node_put(np);
		group->num++;
		if (group->num >= group->hw_max || group->num >= WLA_GROUP_BUF)
			break;
	}
	of_property_read_u32(parent_node, WLA__MON_CH_HW_MAX, &mon->ch_hw_max);
	of_property_read_u32(parent_node, WLA__MON_DDR_STA_MUX_HW_MAX,
							&mon->ddr_sta_mux_hw_max);
	of_property_read_u32(parent_node, WLA__MON_PT_STA_MUX_HW_MAX,
							&mon->pt_sta_mux_hw_max);
	of_property_read_u32(parent_node, WLA__VLC_MUX_HW_MAX,
							&mon->vlc_mux_hw_max);
	of_property_read_u32(parent_node, WLA__TFC_MUX_HW_MAX,
							&mon->tfc_mux_hw_max);
	of_property_read_u32(parent_node, WLA__PMSR_OCLA_MUX_HW_MAX,
							&mon->pmsr_ocla_mux_hw_max);
	of_property_read_u32(parent_node, WLA__DBG_LAT_HW_MAX,
							&lat->lat_hw_max);
	of_property_read_u32(parent_node, WLA__DBG_LAT_DDR_STA_MUX_HW_MAX,
							&lat->ddr_sta_mux_hw_max);
	while ((np = of_parse_phandle(parent_node, WLA__MON_CH_NODE,
									mon->ch_num))) {
		of_property_read_u32(np, WLA__MON_SIG_SEL,
								&mon->ch[mon->ch_num].mux.sig_sel);
		of_property_read_u32(np, WLA__MON_BIT_SEL,
								&mon->ch[mon->ch_num].mux.bit_sel);
		of_property_read_u32(np, WLA__MON_TRIG_TYPE,
								&mon->ch[mon->ch_num].trig_type);
		of_node_put(np);
		mon->ch_num++;
		if (mon->ch_num >= mon->ch_hw_max || mon->ch_num >= WLA_MON_CH_BUF)
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__MON_DDR_STA_MUX_NODE,
									mon->ddr_sta_mux_num))) {
		mon->ddr_sta_mux_num++;
		of_property_read_u32(np, WLA__STA_MUX,
								&mon->ddr_sta_mux[mon->ddr_sta_mux_num]);
		of_node_put(np);
		if (mon->ddr_sta_mux_num >= mon->ddr_sta_mux_hw_max ||
				mon->ddr_sta_mux_num >= (WLA_DDR_STA_MUX_BUF - 1))
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__MON_PT_STA_MUX_NODE,
									mon->pt_sta_mux_num))) {
		mon->pt_sta_mux_num++;
		of_property_read_u32(np, WLA__STA_MUX,
								&mon->pt_sta_mux[mon->pt_sta_mux_num]);
		of_node_put(np);
		if (mon->pt_sta_mux_num >= mon->pt_sta_mux_hw_max ||
				mon->pt_sta_mux_num >= (WLA_PT_STA_MUX_BUF - 1))
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__VLC_MUX_NODE,
									mon->vlc_mux_num))) {
		of_property_read_u32(np, WLA__MON_SIG_SEL,
								&mon->vlc_mux[mon->vlc_mux_num].sig_sel);
		of_property_read_u32(np, WLA__MON_BIT_SEL,
								&mon->vlc_mux[mon->vlc_mux_num].bit_sel);
		of_node_put(np);
		mon->vlc_mux_num++;
		if (mon->vlc_mux_num >= mon->vlc_mux_hw_max ||
				mon->vlc_mux_num >= WLA_VLC_MUX_BUF)
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__TFC_MUX_NODE,
									mon->tfc_mux_num))) {
		of_property_read_u32(np, WLA__MON_SIG_SEL,
								&mon->tfc_mux[mon->tfc_mux_num].sig_sel);
		of_property_read_u32(np, WLA__MON_BIT_SEL,
								&mon->tfc_mux[mon->tfc_mux_num].bit_sel);
		of_node_put(np);
		mon->tfc_mux_num++;
		if (mon->tfc_mux_num >= mon->tfc_mux_hw_max ||
				mon->tfc_mux_num >= WLA_TFC_MUX_BUF)
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__PMSR_OCLA_MUX_NODE,
									mon->pmsr_ocla_mux_num))) {
		of_property_read_u32(np, WLA__MON_SIG_SEL,
						&mon->pmsr_ocla_mux[mon->pmsr_ocla_mux_num].sig_sel);
		of_property_read_u32(np, WLA__MON_BIT_SEL,
						&mon->pmsr_ocla_mux[mon->pmsr_ocla_mux_num].bit_sel);
		of_node_put(np);
		mon->pmsr_ocla_mux_num++;
		if (mon->pmsr_ocla_mux_num >= mon->pmsr_ocla_mux_hw_max ||
				mon->pmsr_ocla_mux_num >= WLA_PMSR_OCLA_MUX_BUF)
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__DBG_LAT_NODE,
									lat->lat_num))) {
		of_property_read_u32(np, WLA__DBG_LATCH_SEL,
								&lat->lat_sel[lat->lat_num]);
		of_node_put(np);
		lat->lat_num++;
		if (lat->lat_num >= lat->lat_hw_max ||
				lat->lat_num >= WLA_DBG_LATCH_BUF)
			break;
	}
	while ((np = of_parse_phandle(parent_node, WLA__DBG_LAT_DDR_STA_MUX_NODE,
									lat->ddr_sta_mux_num))) {
		lat->ddr_sta_mux_num++;
		of_property_read_u32(np, WLA__STA_MUX,
								&lat->ddr_sta_mux[lat->ddr_sta_mux_num]);
		of_node_put(np);
		if (lat->ddr_sta_mux_num >= lat->ddr_sta_mux_hw_max ||
				lat->ddr_sta_mux_num >= (WLA_DBG_LAT_DDR_STA_MUX_BUF - 1))
			break;
	}

	of_property_read_u32(parent_node, WLA__LBC_SHUCLR_CNT,
					&rglt->lbc.shuclr_cnt);
	of_property_read_u32(parent_node, WLA__LBC_SHUETR_THRES,
					&rglt->lbc.shuetr_thres);
	of_property_read_u32(parent_node, WLA__LBC_SHUEXT_THRES,
					&rglt->lbc.shuext_thres);
	of_property_read_u64(parent_node, WLA__LBC_MST_URG_MSK,
					&rglt->lbc.urg_msk);

	buf = of_get_property(parent_node, WLA__LBC_SHU_LB_LVL, &len);
	len /= sizeof(*buf);
	if (buf && (len == WLA_RGLT_LBC_SHU_NUM * WLA_RGLT_LBC_SHU_LVL_NUM)) {
		for (i = 0; i < WLA_RGLT_LBC_SHU_NUM; i++) {
			for (j = 0; j < WLA_RGLT_LBC_SHU_LVL_NUM; j++) {
				rglt->lbc.shu[i].lvl[j] =
					be32_to_cpu(buf[i * WLA_RGLT_LBC_SHU_LVL_NUM + j]);
			}
		}
	} else
		pr_info("[mtk-wla] wla lbc data failed\n");

	of_property_read_u32(parent_node, WLA__PLC_EN, &rglt->plc.enable);
	of_property_read_u32(parent_node, WLA__PLC_DIS_TARGET_VAL,
							&rglt->plc.dis_trg_val);
	of_property_read_u32(parent_node, WLA__PLC_HIT_MISS_THRES,
							&rglt->plc.hit_miss_thres);

	of_property_read_u32(parent_node, WLA__TFC_HIGHBOUND,
					&rglt->tfc.highbound);
	of_property_read_u32(parent_node, WLA__TFC_YLW_TIMEOUT,
					&rglt->tfc.ylw_timeout);
	of_property_read_u64(parent_node, WLA__TFC_MST_REQ_MSK,
					&rglt->tfc.req_msk);
	of_property_read_u64(parent_node, WLA__TFC_MST_URG_MSK,
					&rglt->tfc.urg_msk);

	buf = of_get_property(parent_node, WLA__BUS_PROT_INST_LIST, &bus_prot.len);
	bus_prot.len /= sizeof(*buf);
	if (buf && (bus_prot.len <= WLA_BUS_PROT_BUF)) {
		for (i = 0; i < bus_prot.len; i++)
			bus_prot.buf[i] = be32_to_cpu(buf[i]);
	} else
		pr_info("[mtk-wla] bus prot inst node fail\n");

	ret = wla_init(init_setting_ver, ddr_ctrl, rglt, &bus_prot);
	if (ret)
		pr_info("[mtk-wla] wla init failed\n");
	ret = wla_mon_init(mon);
	if (ret)
		pr_info("[mtk-wla] wla mon init failed\n");
	ret = wla_dbg_latch_init(lat);
	if (ret)
		pr_info("[mtk-wla] wla dbg latch init failed\n");
	pr_info("[mtk-wla] wla init done\n");

	return WLA_SUCCESS;
}

static const struct of_device_id wla_of_match[] = {
	{
		.compatible = "mediatek,wla",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, wla_of_match);

static struct platform_driver wla_driver = {
	.driver = {
		.name = "wla",
		.of_match_table = wla_of_match,
	},
	.probe	= wla_probe,
};
module_platform_driver(wla_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK work load aware - v1");
MODULE_AUTHOR("MediaTek Inc.");
