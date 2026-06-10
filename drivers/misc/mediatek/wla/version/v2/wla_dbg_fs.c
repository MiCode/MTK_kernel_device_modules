// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/string.h>

#include <wla_dbg_sysfs.h>
#include "wla.h"

#define WLA_GROUP_OP(op, _priv) ({\
	op.fs_read = wla_group_read;\
	op.fs_write = wla_group_write;\
	op.priv = _priv; })

#define WLA_GROUP_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_GROUP_OP(_n.op, &_n); })

#define WLA_LBC_OP(op, _priv) ({\
	op.fs_read = wla_lbc_read;\
	op.fs_write = wla_lbc_write;\
	op.priv = _priv; })

#define WLA_LBC_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_LBC_OP(_n.op, &_n); })

#define WLA_PLC_OP(op, _priv) ({\
	op.fs_read = wla_plc_read;\
	op.fs_write = wla_plc_write;\
	op.priv = _priv; })

#define WLA_PLC_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_PLC_OP(_n.op, &_n); })

#define WLA_TFC_OP(op, _priv) ({\
	op.fs_read = wla_tfc_read;\
	op.fs_write = wla_tfc_write;\
	op.priv = _priv; })

#define WLA_TFC_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_TFC_OP(_n.op, &_n); })

#define WLA_MON_OP(op, _priv) ({\
	op.fs_read = wla_mon_read;\
	op.fs_write = wla_mon_write;\
	op.priv = _priv; })

#define WLA_MON_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_MON_OP(_n.op, &_n); })

#define WLA_DBG_LATCH_OP(op, _priv) ({\
	op.fs_read = wla_dbg_latch_read;\
	op.fs_write = wla_dbg_latch_write;\
	op.priv = _priv; })

#define WLA_DBG_LATCH_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	WLA_DBG_LATCH_OP(_n.op, &_n); })

enum {
	TYPE_MASTER_SEL,
	TYPE_DEBOUNCE,
	TYPE_STRATEGY,
	TYPE_IGNORE_GROUP,

	NF_TYPE_GROUP_MAX
};

enum {
	TYPE_LBC_SHUCLR_CNT,
	TYPE_LBC_SHUETR_THRES,
	TYPE_LBC_SHUEXT_THRES,
	TYPE_LBC_URG_MASK,
	TYPE_LBC_SHU0_LVL,
	TYPE_LBC_SHU1_LVL,

	NF_TYPE_LBC_MAX
};

enum {
	TYPE_PLC_EN,
	TYPE_PLC_DIS_TARGET_VAL,
	TYPE_PLC_HIT_MISS_THRES,

	NF_TYPE_PLC_MAX
};

enum {
	TYPE_TFC_HIGHBOUND,
	TYPE_TFC_YLW_TIMEOUT,
	TYPE_TFC_REQ_MASK,
	TYPE_TFC_URG_MASK,

	NF_TYPE_TFC_MAX
};

enum {
	TYPE_START,
	TYPE_CH_CNT,
	TYPE_MON_SEL,
	TYPE_DDR_STA_MUX,
	TYPE_PT_STA_MUX,
	TYPE_VLC_MUX,
	TYPE_TFC_MUX,
	TYPE_PMSR_OCLA_MUX,

	NF_TYPE_MON_MAX
};

enum {
	TYPE_LAT_SEL,
	TYPE_LAT_STA_MUX,

	NF_TYPE_DBG_LATCH_MAX
};

static struct wla_sysfs_handle wla_dbg_ddren_ctrl;
static struct wla_sysfs_handle wla_entry_dbg_group;
static struct wla_sysfs_handle wla_dbg_rglt2p0;
static struct wla_sysfs_handle wla_dbg_lbc;
static struct wla_sysfs_handle wla_dbg_plc;
static struct wla_sysfs_handle wla_dbg_tfc;
static struct wla_sysfs_handle wla_entry_dbg_monitor;
static struct wla_sysfs_handle wla_dbg_latch;

static struct WLA_DBG_NODE master_sel;
static struct WLA_DBG_NODE debounce;
static struct WLA_DBG_NODE strategy;
static struct WLA_DBG_NODE ignore_urgent;

static struct WLA_DBG_NODE shuclr_cnt;
static struct WLA_DBG_NODE shuetr_thres;
static struct WLA_DBG_NODE shuext_thres;
static struct WLA_DBG_NODE lbc_urg_mask;
static struct WLA_DBG_NODE shu0_lvl;
static struct WLA_DBG_NODE shu1_lvl;

static struct WLA_DBG_NODE plc_en;
static struct WLA_DBG_NODE plc_dis_target_val;
static struct WLA_DBG_NODE hit_miss_thres;

static struct WLA_DBG_NODE highbound;
static struct WLA_DBG_NODE ylw_timeout;
static struct WLA_DBG_NODE tfc_req_mask;
static struct WLA_DBG_NODE tfc_urg_mask;

static struct WLA_DBG_NODE start;
static struct WLA_DBG_NODE ch_cnt;
static struct WLA_DBG_NODE mon_sel;
static struct WLA_DBG_NODE ddr_sta_mux;
static struct WLA_DBG_NODE pt_sta_mux;
static struct WLA_DBG_NODE vlc_mux;
static struct WLA_DBG_NODE tfc_mux;
static struct WLA_DBG_NODE pmsr_ocla_mux;

static struct WLA_DBG_NODE lat_sel;
static struct WLA_DBG_NODE lat_sta_mux;

static ssize_t wla_mon_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int idx, num;
	int ret;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_CH_CNT: {
		uint32_t cnt = 0;
		uint32_t req_low = 0, hws1_req = 0, slc_ddren_low = 0;

		num = wla_mon_get_ch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret = wla_mon_get_ch_cnt(idx, &cnt, NULL);
			if (!ret)
				wla_dbg_log("ch_%d cnt = %d\n",
								idx, cnt);
			else
				wla_dbg_log("read fail\n");
		}
		ret = wla_mon_get_ch_cnt(0, &req_low, NULL);
		ret |= wla_mon_get_ch_cnt(1, &hws1_req, NULL);
		ret |= wla_mon_get_ch_cnt(3, &slc_ddren_low, NULL);
		if (!ret) {
			wla_dbg_log("normal s1 counter = %d\n", req_low);
			wla_dbg_log("extra s1 counter = %d\n",
							(hws1_req > req_low) ? hws1_req - req_low : 0);
			wla_dbg_log("slc_ddren_req low cnt = %d\n", slc_ddren_low);
		} else
			wla_dbg_log("read fail\n");
		wla_mon_disable();
		break;
	}
	case TYPE_MON_SEL: {
		struct wla_mon_ch_setting ch_set = {0};

		num = wla_mon_get_ch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret =  wla_mon_get_ch_sel(idx, &ch_set);
			if (!ret)
				wla_dbg_log(
					"ch_%d sig_sel = %d, bit_sel = %d, trig_type = 0x%x\n",
					idx, ch_set.mux.sig_sel, ch_set.mux.bit_sel,
					ch_set.trig_type);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_DDR_STA_MUX: {
		unsigned int mux = 0;

		num = wla_mon_get_ddr_sta_mux_hw_max();
		for (idx = 1; idx <= num; idx++) {
			ret =  wla_mon_get_ddr_sta_mux(idx, &mux);
			if (!ret)
				wla_dbg_log("sta_%d mux = 0x%x\n", idx, mux);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_PT_STA_MUX: {
		unsigned int mux = 0;

		num = wla_mon_get_pt_sta_mux_hw_max();
		for (idx = 1; idx <= num; idx++) {
			ret =  wla_mon_get_pt_sta_mux(idx, &mux);
			if (!ret)
				wla_dbg_log("sta_%d mux = 0x%x\n", idx, mux);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_VLC_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		num = wla_mon_get_vlc_mux_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret =  wla_mon_get_vlc_mux(idx, &mux_sel);
			if (!ret)
				wla_dbg_log("vlc_%d sig_sel = %d, bit_sel = %d\n",
							idx, mux_sel.sig_sel, mux_sel.bit_sel);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_TFC_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		num = wla_mon_get_tfc_mux_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret =  wla_mon_get_tfc_mux(idx, &mux_sel);
			if (!ret)
				wla_dbg_log("tfc_%d sig_sel = %d, bit_sel = %d\n",
							idx, mux_sel.sig_sel, mux_sel.bit_sel);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_PMSR_OCLA_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		num = wla_mon_get_pmsr_ocla_mux_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret =  wla_mon_get_pmsr_ocla_mux(idx, &mux_sel);
			if (!ret)
				wla_dbg_log("pmsr_ocla_%d sig_sel = %d, bit_sel = %d\n",
							idx, mux_sel.sig_sel, mux_sel.bit_sel);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_mon_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int para1 = 0;
	char *token;
	char *str = FromUserBuf;
	const char *delim = " ";
	int ret;

	if (!str || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_START:
		/* para1: win_len_sec */
		if (kstrtouint(str, 10, &para1))
			return -EINVAL;
		ret = wla_mon_start(para1);
		if (ret)
			pr_info("input must <= 150s\n");
		break;
	case TYPE_MON_SEL: {
		struct wla_mon_ch_setting ch_set = {0};

		/* para1: channel */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &ch_set.mux.sig_sel))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &ch_set.mux.bit_sel))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 16, &ch_set.trig_type))
			return -EINVAL;

		wla_mon_ch_sel(para1, &ch_set);
		break;
	}
	case TYPE_DDR_STA_MUX: {
		unsigned int mux = 0;

		/* para1: status_n */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 16, &mux))
			return -EINVAL;

		wla_mon_set_ddr_sta_mux(para1, mux);
		break;
	}
	case TYPE_PT_STA_MUX: {
		unsigned int mux = 0;

		/* para1: status_n */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 16, &mux))
			return -EINVAL;

		wla_mon_set_pt_sta_mux(para1, mux);
		break;
	}
	case TYPE_VLC_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		/* para1: mux_n */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.sig_sel))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.bit_sel))
			return -EINVAL;

		wla_mon_set_vlc_mux(para1, &mux_sel);
		break;
	}
	case TYPE_TFC_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		/* para1: mux_n */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.sig_sel))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.bit_sel))
			return -EINVAL;

		wla_mon_set_tfc_mux(para1, &mux_sel);
		break;
	}
	case TYPE_PMSR_OCLA_MUX: {
		struct wla_mon_mux_sel mux_sel = {0};

		/* para1: mux_n */
		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &para1))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.sig_sel))
			return -EINVAL;

		token = strsep(&str, delim);
		if (!token)
			return -EINVAL;
		if (kstrtouint(token, 10, &mux_sel.bit_sel))
			return -EINVAL;

		wla_mon_set_pmsr_ocla_mux(para1, &mux_sel);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_group_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int grp, grp_num;
	unsigned int data = 0;
	uint64_t master;
	int ret;

	if (!p || !node)
		return -EINVAL;

	grp_num = wla_get_group_num();
	switch (node->type) {
	case TYPE_MASTER_SEL:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_mst(grp, &master);
			if (!ret)
				wla_dbg_log("group_mst_%d = 0x%llx\n", grp, master);
			else
				wla_dbg_log("read group_%d fail\n", grp);
		}
		break;

	case TYPE_DEBOUNCE:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_debounce(grp, &data);
			if (!ret)
				wla_dbg_log("group_debounce_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	case TYPE_STRATEGY:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_strategy(grp, &data);
			if (!ret)
				wla_dbg_log("group_strategy_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	case TYPE_IGNORE_GROUP:
		for (grp = 0; grp < grp_num; grp++) {
			ret = wla_get_group_ignore_urg(grp, &data);
			if (!ret)
				wla_dbg_log("group_ignore_urg_%d = 0x%x\n", grp, data);
			else
				wla_dbg_log(" read group_%d fail\n", grp);
		}
		break;

	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_group_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int grp = 0;
	uint64_t para = 0;
	char *token;
	char *str = FromUserBuf;
	const char *delim = " ";

	if (!str || !node)
		return -EINVAL;

	token = strsep(&str, delim);
	if (!token)
		return -EINVAL;
	if (kstrtouint(token, 10, &grp))
		return -EINVAL;

	token = strsep(&str, delim);
	if (!token)
		return -EINVAL;
	if (kstrtoull(token, 16, &para))
		return -EINVAL;

	switch (node->type) {
	case TYPE_MASTER_SEL:
		wla_set_group_mst(grp, para);
		break;

	case TYPE_DEBOUNCE:
		wla_set_group_debounce(grp, (uint32_t)para);
		break;

	case TYPE_STRATEGY:
		wla_set_group_strategy(grp, (uint32_t)para);
		break;

	case TYPE_IGNORE_GROUP:
		wla_set_group_ignore_urg(grp, (uint32_t)para);
		break;

	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_lbc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_LBC_SHUCLR_CNT: {
		unsigned int val;

		val = wla_get_lbc_shuclr_cnt();
		wla_dbg_log("[mtk-wla] wla_lbc_shuclr_cnt = 0x%x\n", val);
		break;
	}
	case TYPE_LBC_SHUETR_THRES: {
		unsigned int val;

		val = wla_get_lbc_shuetr_thres();
		wla_dbg_log("[mtk-wla] wla_lbc_shuetr_thres = 0x%x\n", val);
		break;
	}
	case TYPE_LBC_SHUEXT_THRES: {
		unsigned int val;

		val = wla_get_lbc_shuext_thres();
		wla_dbg_log("[mtk-wla] wla_lbc_shuext_thres = 0x%x\n", val);
		break;
	}
	case TYPE_LBC_URG_MASK: {
		uint64_t mask;

		mask = wla_get_lbc_urgent_mask();
		wla_dbg_log("[mtk-wla] wla_lbc_urgent_mask = 0x%llx\n", mask);
		break;
	}
	case TYPE_LBC_SHU0_LVL: {
		struct wla_lbc_lvl_arr lv = {0};
		int ret;

		ret = wla_get_lbc_lb_lvl(&lv, 0);
		if (!ret) {
			wla_dbg_log("[mtk-wla] wla_get_lbc_lb_lvl_0\n");
			wla_dbg_log("0x%x 0x%x 0x%x\n", lv.lvl[0], lv.lvl[1], lv.lvl[2]);
			wla_dbg_log("0x%x 0x%x 0x%x\n", lv.lvl[3], lv.lvl[4], lv.lvl[5]);
			wla_dbg_log("0x%x 0x%x\n", lv.lvl[6], lv.lvl[7]);
		} else
			wla_dbg_log("[mtk-wla] wla_get_lbc_lb_lvl fail\n");
		break;
	}
	case TYPE_LBC_SHU1_LVL: {
		struct wla_lbc_lvl_arr lv = {0};
		int ret;

		ret = wla_get_lbc_lb_lvl(&lv, 1);
		if (!ret) {
			wla_dbg_log("[mtk-wla] wla_get_lbc_lb_lvl_1\n");
			wla_dbg_log("0x%x 0x%x 0x%x\n", lv.lvl[0], lv.lvl[1], lv.lvl[2]);
			wla_dbg_log("0x%x 0x%x 0x%x\n", lv.lvl[3], lv.lvl[4], lv.lvl[5]);
			wla_dbg_log("0x%x 0x%x\n", lv.lvl[6], lv.lvl[7]);
		} else
			wla_dbg_log("[mtk-wla] wla_get_lbc_lb_lvl fail\n");
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_lbc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;

	if (!FromUserBuf || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_LBC_SHUCLR_CNT: {
		unsigned int val = 0;

		if (kstrtouint(FromUserBuf, 16, &val))
			return -EINVAL;
		wla_set_lbc_shuclr_cnt(val);
		pr_info("[mtk-wla] wla_set_lbc_shuclr_cnt 0x%x\n", val);
		break;
	}
	case TYPE_LBC_SHUETR_THRES: {
		unsigned int val = 0;

		if (kstrtouint(FromUserBuf, 16, &val))
			return -EINVAL;
		wla_set_lbc_shuetr_thres(val);
		pr_info("[mtk-wla] wla_set_lbc_shuetr_thres 0x%x\n", val);
		break;
	}
	case TYPE_LBC_SHUEXT_THRES: {
		unsigned int val = 0;

		if (kstrtouint(FromUserBuf, 16, &val))
			return -EINVAL;
		wla_set_lbc_shuext_thres(val);
		pr_info("[mtk-wla] wla_set_lbc_shuext_thres 0x%x\n", val);
		break;
	}
	case TYPE_LBC_URG_MASK: {
		uint64_t mask = 0;

		if (kstrtoull(FromUserBuf, 16, &mask))
			return -EINVAL;
		wla_set_lbc_urgent_mask(mask);
		pr_info("[mtk-wla] wla_set_lbc_urgent_mask 0x%llx\n", mask);
		break;
	}
	case TYPE_LBC_SHU0_LVL: {
		struct wla_lbc_lvl_arr lv = {0};
		char *str = FromUserBuf;
		char *token;
		const char *delim = " ";
		unsigned int i = 0;

		while ((token = strsep(&str, delim)) != NULL) {
			if (kstrtouint(token, 16, &lv.lvl[i]))
				return -EINVAL;

			if (++i >= 8)
				break;
		}

		if (i != 8)
			return -EINVAL;

		wla_set_lbc_lb_lvl(&lv, 0);
		pr_info("[mtk-wla] wla_set_lbc_lb_lvl_0\n");
		pr_info("[mtk-wla] 0x%x 0x%x 0x%x\n", lv.lvl[0], lv.lvl[1], lv.lvl[2]);
		pr_info("[mtk-wla] 0x%x 0x%x 0x%x\n", lv.lvl[3], lv.lvl[4], lv.lvl[5]);
		pr_info("[mtk-wla] 0x%x 0x%x\n", lv.lvl[6], lv.lvl[7]);
		break;
	}
	case TYPE_LBC_SHU1_LVL: {
		struct wla_lbc_lvl_arr lv = {0};
		char *str = FromUserBuf;
		char *token;
		const char *delim = " ";
		unsigned int i = 0;

		while ((token = strsep(&str, delim)) != NULL) {
			if (kstrtouint(token, 16, &lv.lvl[i]))
				return -EINVAL;

			if (++i >= 8)
				break;
		}

		if (i != 8)
			return -EINVAL;

		wla_set_lbc_lb_lvl(&lv, 1);
		pr_info("[mtk-wla] wla_set_lbc_lb_lvl_1\n");
		pr_info("[mtk-wla] 0x%x 0x%x 0x%x\n", lv.lvl[0], lv.lvl[1], lv.lvl[2]);
		pr_info("[mtk-wla] 0x%x 0x%x 0x%x\n", lv.lvl[3], lv.lvl[4], lv.lvl[5]);
		pr_info("[mtk-wla] 0x%x 0x%x\n", lv.lvl[6], lv.lvl[7]);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_plc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int val;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_PLC_EN:
		val = wla_get_plc_en();
		wla_dbg_log("[mtk-wla] wla_get_plc_en = 0x%x\n", val);
		break;
	case TYPE_PLC_DIS_TARGET_VAL:
		val = wla_get_plc_dis_trg_val();
		wla_dbg_log("[mtk-wla] wla_get_plc_dis_target_val = 0x%x\n", val);
		break;
	case TYPE_PLC_HIT_MISS_THRES:
		val = wla_get_plc_hit_miss_thres();
		wla_dbg_log("[mtk-wla] wla_get_plc_hit_miss_thres = 0x%x\n", val);
		break;
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_plc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int val = 0;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (kstrtouint(FromUserBuf, 16, &val))
		return -EINVAL;

	switch (node->type) {
	case TYPE_PLC_EN:
		wla_set_plc_en(val);
		pr_info("[mtk-wla] wla_set_plc_en 0x%x\n", val);
		break;
	case TYPE_PLC_DIS_TARGET_VAL:
		wla_set_plc_dis_trg_val(val);
		pr_info("[mtk-wla] wla_set_plc_dis_target_val 0x%x\n", val);
		break;
	case TYPE_PLC_HIT_MISS_THRES:
		wla_set_plc_hit_miss_thres(val);
		pr_info("[mtk-wla] wla_set_plc_hit_miss_thres 0x%x\n", val);
		break;
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_tfc_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_TFC_HIGHBOUND: {
		unsigned int val;

		val = wla_get_tfc_highbound();
		wla_dbg_log("[mtk-wla] wla_get_tfc_highbound = 0x%x\n", val);
		break;
	}
	case TYPE_TFC_YLW_TIMEOUT: {
		unsigned int val;

		val = wla_get_tfc_ylw_timeout();
		wla_dbg_log("[mtk-wla] wla_get_tfc_ylw_timeout = 0x%x\n", val);
		break;
	}
	case TYPE_TFC_REQ_MASK: {
		uint64_t mask;

		mask = wla_get_tfc_req_mask();
		wla_dbg_log("[mtk-wla] wla_get_tfc_req_mask = 0x%llx\n", mask);
		break;
	}
	case TYPE_TFC_URG_MASK: {
		uint64_t mask;

		mask = wla_get_tfc_urg_mask();
		wla_dbg_log("[mtk-wla] wla_get_tfc_req_mask = 0x%llx\n", mask);
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_tfc_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;

	if (!FromUserBuf || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_TFC_HIGHBOUND: {
		unsigned int val = 0;

		if (kstrtouint(FromUserBuf, 16, &val))
			return -EINVAL;
		wla_set_tfc_highbound(val);
		pr_info("[mtk-wla] wla_set_tfc_highbound 0x%x\n", val);
		break;
	}
	case TYPE_TFC_YLW_TIMEOUT: {
		unsigned int val = 0;

		if (kstrtouint(FromUserBuf, 16, &val))
			return -EINVAL;
		wla_set_tfc_ylw_timeout(val);
		pr_info("[mtk-wla] wla_set_tfc_ylw_timeout 0x%x\n", val);
		break;
	}
	case TYPE_TFC_REQ_MASK: {
		uint64_t mask = 0;

		if (kstrtoull(FromUserBuf, 16, &mask))
			return -EINVAL;
		wla_set_tfc_req_mask(mask);
		pr_info("[mtk-wla] wla_set_tfc_req_mask 0x%llx\n", mask);
		break;
	}
	case TYPE_TFC_URG_MASK: {
		uint64_t mask = 0;

		if (kstrtoull(FromUserBuf, 16, &mask))
			return -EINVAL;
		wla_set_tfc_urg_mask(mask);
		pr_info("[mtk-wla] wla_set_tfc_req_mask 0x%llx\n", mask);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_dbg_latch_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct WLA_DBG_NODE *node =
			(struct WLA_DBG_NODE *)priv;
	unsigned int idx, num;
	int ret;

	if (!p || !node)
		return -EINVAL;

	switch (node->type) {
	case TYPE_LAT_SEL: {
		unsigned int sel = 0;

		num = wla_get_dbg_latch_hw_max();
		for (idx = 0; idx < num; idx++) {
			ret = wla_get_dbg_latch_sel(idx, &sel);
			if (!ret)
				wla_dbg_log("latch_%d_sel = 0x%x\n", idx, sel);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	case TYPE_LAT_STA_MUX: {
		unsigned int mux = 0;

		num = wla_get_dbg_lat_ddr_sta_mux_hw_max();
		for (idx = 1; idx <= num; idx++) {
			ret =  wla_get_dbg_lat_ddr_sta_mux(idx, &mux);
			if (!ret)
				wla_dbg_log("sta_%d mux = 0x%x\n", idx, mux);
			else
				wla_dbg_log("read fail\n");
		}
		break;
	}
	default:
		wla_dbg_log("unknown command\n");
		break;
	}

	return p - ToUserBuf;
}

static ssize_t wla_dbg_latch_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct WLA_DBG_NODE *node =
				(struct WLA_DBG_NODE *)priv;
	unsigned int para1 = 0, para2 = 0;
	char *token;
	char *str = FromUserBuf;
	const char *delim = " ";

	if (!str || !node)
		return -EINVAL;

	token = strsep(&str, delim);
	if (!token)
		return -EINVAL;
	if (kstrtouint(token, 10, &para1))
		return -EINVAL;

	token = strsep(&str, delim);
	if (!token)
		return -EINVAL;
	if (kstrtouint(token, 16, &para2))
		return -EINVAL;

	switch (node->type) {
	case TYPE_LAT_SEL: {
		/*
		 * para1: latch num
		 * para2: latch sel
		 */
		wla_set_dbg_latch_sel(para1, para2);
		break;
	}
	case TYPE_LAT_STA_MUX: {
		/*
		 * para1: ddr_sta_num
		 * para2: ddr_sta_mux
		 */
		wla_set_dbg_lat_ddr_sta_mux(para1, para2);
		break;
	}
	default:
		pr_info("unknown command\n");
		break;
	}

	return sz;
}

static ssize_t wla_ddren_bypass_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int bypass = 1;

	if (!FromUserBuf)
		return -EINVAL;
	/* adb cmd example: */
	/* "echo 1 > /proc/wla/wla_dbg/ddren_bypass" */

	/* para1: bypass */
	if (!kstrtouint(FromUserBuf, 10, &bypass)) {
		wla_set_ddren_bypass(bypass);
		return sz;
	}
	return -EINVAL;
}

static ssize_t wla_ddren_bypass_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int bypass = 1;

	if (!p)
		return -EINVAL;

	bypass = wla_get_ddren_bypass();
	wla_dbg_log("WLA ddren bypass = %d\n", bypass);

	return p - ToUserBuf;
}

static ssize_t wla_rglt2p0_bypass_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int bypass = 1;

	if (!FromUserBuf)
		return -EINVAL;
	/* adb cmd example: */
	/* "echo 1 > /proc/wla/wla_dbg/rglt2p0_bypass" */

	/* para1: bypass */
	if (!kstrtouint(FromUserBuf, 10, &bypass)) {
		wla_set_rglt2p0_bypass(bypass);
		return sz;
	}
	return -EINVAL;
}

static ssize_t wla_rglt2p0_bypass_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int bypass = 1;

	if (!p)
		return -EINVAL;

	bypass = wla_get_rglt2p0_bypass();
	wla_dbg_log("WLA rglt2p0 bypass = %d\n", bypass);

	return p - ToUserBuf;
}

static ssize_t wla_ddren_force_on_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int force_on = 1;

	if (!FromUserBuf)
		return -EINVAL;
	/* adb cmd example: */
	/* "echo 1 > /proc/wla/wla_dbg/ddren_ctrl/ddren_force_on" */

	/* para1: force_on */
	if (!kstrtouint(FromUserBuf, 10, &force_on)) {
		wla_set_ddren_force_on(force_on);
		return sz;
	}
	return -EINVAL;
}

static ssize_t wla_ddren_force_on_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	unsigned int force_on = 1;
	int ret;

	if (!p)
		return -EINVAL;

	ret = wla_get_ddren_force_on(&force_on);
	if (ret)
		wla_dbg_log("WLA ddren force on read failed.\n");
	else
		wla_dbg_log("WLA ddren force on = %d\n", force_on);

	return p - ToUserBuf;
}

static const struct wla_sysfs_op wla_ddren_bypass_fops = {
	.fs_read = wla_ddren_bypass_read,
	.fs_write = wla_ddren_bypass_write,
};

static const struct wla_sysfs_op wla_rglt2p0_bypass_fops = {
	.fs_read = wla_rglt2p0_bypass_read,
	.fs_write = wla_rglt2p0_bypass_write,
};

static const struct wla_sysfs_op wla_ddren_force_on_fops = {
	.fs_read = wla_ddren_force_on_read,
	.fs_write = wla_ddren_force_on_write,
};

static int __init wla_fs_init(void)
{
	wla_dbg_sysfs_root_entry_create();
	wla_dbg_sysfs_entry_node_add("ddren_bypass", WLA_DBG_SYS_FS_MODE,
				&wla_ddren_bypass_fops, NULL);

	wla_dbg_sysfs_entry_node_add("rglt2p0_bypass", WLA_DBG_SYS_FS_MODE,
				&wla_rglt2p0_bypass_fops, NULL);

	wla_dbg_sysfs_sub_entry_add("ddren_ctrl", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_dbg_ddren_ctrl);

	wla_dbg_sysfs_sub_entry_node_add("ddren_force_on",
					WLA_DBG_SYS_FS_MODE,
					&wla_ddren_force_on_fops,
					&wla_dbg_ddren_ctrl,
					NULL);

	wla_dbg_sysfs_sub_entry_add("group", WLA_DBG_SYS_FS_MODE,
				&wla_dbg_ddren_ctrl, &wla_entry_dbg_group);

	WLA_GROUP_NODE_INIT(master_sel, "master_sel",
				    TYPE_MASTER_SEL);
	wla_dbg_sysfs_sub_entry_node_add(master_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&master_sel.op,
					&wla_entry_dbg_group,
					&master_sel.handle);

	WLA_GROUP_NODE_INIT(debounce, "debounce",
				    TYPE_DEBOUNCE);
	wla_dbg_sysfs_sub_entry_node_add(debounce.name,
					WLA_DBG_SYS_FS_MODE,
					&debounce.op,
					&wla_entry_dbg_group,
					&debounce.handle);

	WLA_GROUP_NODE_INIT(strategy, "strategy",
				    TYPE_STRATEGY);
	wla_dbg_sysfs_sub_entry_node_add(strategy.name,
					WLA_DBG_SYS_FS_MODE,
					&strategy.op,
					&wla_entry_dbg_group,
					&strategy.handle);

	WLA_GROUP_NODE_INIT(ignore_urgent, "ignore_urgent",
				    TYPE_IGNORE_GROUP);
	wla_dbg_sysfs_sub_entry_node_add(ignore_urgent.name,
					WLA_DBG_SYS_FS_MODE,
					&ignore_urgent.op,
					&wla_entry_dbg_group,
					&ignore_urgent.handle);

	wla_dbg_sysfs_sub_entry_add("rglt2p0", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_dbg_rglt2p0);

	wla_dbg_sysfs_sub_entry_add("lbc", WLA_DBG_SYS_FS_MODE,
				&wla_dbg_rglt2p0, &wla_dbg_lbc);

	WLA_LBC_NODE_INIT(shuclr_cnt, "shuclr_cnt",
				    TYPE_LBC_SHUCLR_CNT);
	wla_dbg_sysfs_sub_entry_node_add(shuclr_cnt.name,
					WLA_DBG_SYS_FS_MODE,
					&shuclr_cnt.op,
					&wla_dbg_lbc,
					&shuclr_cnt.handle);

	WLA_LBC_NODE_INIT(shuetr_thres, "shuetr_thres",
				    TYPE_LBC_SHUETR_THRES);
	wla_dbg_sysfs_sub_entry_node_add(shuetr_thres.name,
					WLA_DBG_SYS_FS_MODE,
					&shuetr_thres.op,
					&wla_dbg_lbc,
					&shuetr_thres.handle);

	WLA_LBC_NODE_INIT(shuext_thres, "shuext_thres",
				    TYPE_LBC_SHUEXT_THRES);
	wla_dbg_sysfs_sub_entry_node_add(shuext_thres.name,
					WLA_DBG_SYS_FS_MODE,
					&shuext_thres.op,
					&wla_dbg_lbc,
					&shuext_thres.handle);

	WLA_LBC_NODE_INIT(lbc_urg_mask, "urg_mask",
				    TYPE_LBC_URG_MASK);
	wla_dbg_sysfs_sub_entry_node_add(lbc_urg_mask.name,
					WLA_DBG_SYS_FS_MODE,
					&lbc_urg_mask.op,
					&wla_dbg_lbc,
					&lbc_urg_mask.handle);

	WLA_LBC_NODE_INIT(shu0_lvl, "shu0_lvl",
				    TYPE_LBC_SHU0_LVL);
	wla_dbg_sysfs_sub_entry_node_add(shu0_lvl.name,
					WLA_DBG_SYS_FS_MODE,
					&shu0_lvl.op,
					&wla_dbg_lbc,
					&shu0_lvl.handle);

	WLA_LBC_NODE_INIT(shu1_lvl, "shu1_lvl",
				    TYPE_LBC_SHU1_LVL);
	wla_dbg_sysfs_sub_entry_node_add(shu1_lvl.name,
					WLA_DBG_SYS_FS_MODE,
					&shu1_lvl.op,
					&wla_dbg_lbc,
					&shu1_lvl.handle);

	wla_dbg_sysfs_sub_entry_add("plc", WLA_DBG_SYS_FS_MODE,
				&wla_dbg_rglt2p0, &wla_dbg_plc);

	WLA_PLC_NODE_INIT(plc_en, "enable",
				    TYPE_PLC_EN);
	wla_dbg_sysfs_sub_entry_node_add(plc_en.name,
					WLA_DBG_SYS_FS_MODE,
					&plc_en.op,
					&wla_dbg_plc,
					&plc_en.handle);

	WLA_PLC_NODE_INIT(plc_dis_target_val, "plc_dis_target_val",
				    TYPE_PLC_DIS_TARGET_VAL);
	wla_dbg_sysfs_sub_entry_node_add(plc_dis_target_val.name,
					WLA_DBG_SYS_FS_MODE,
					&plc_dis_target_val.op,
					&wla_dbg_plc,
					&plc_dis_target_val.handle);

	WLA_PLC_NODE_INIT(hit_miss_thres, "hit_miss_thres",
				    TYPE_PLC_HIT_MISS_THRES);
	wla_dbg_sysfs_sub_entry_node_add(hit_miss_thres.name,
					WLA_DBG_SYS_FS_MODE,
					&hit_miss_thres.op,
					&wla_dbg_plc,
					&hit_miss_thres.handle);

	wla_dbg_sysfs_sub_entry_add("tfc", WLA_DBG_SYS_FS_MODE,
				&wla_dbg_rglt2p0, &wla_dbg_tfc);

	WLA_TFC_NODE_INIT(highbound, "highbound",
				    TYPE_TFC_HIGHBOUND);
	wla_dbg_sysfs_sub_entry_node_add(highbound.name,
					WLA_DBG_SYS_FS_MODE,
					&highbound.op,
					&wla_dbg_tfc,
					&highbound.handle);

	WLA_TFC_NODE_INIT(ylw_timeout, "ylw_timeout",
				    TYPE_TFC_YLW_TIMEOUT);
	wla_dbg_sysfs_sub_entry_node_add(ylw_timeout.name,
					WLA_DBG_SYS_FS_MODE,
					&ylw_timeout.op,
					&wla_dbg_tfc,
					&ylw_timeout.handle);

	WLA_TFC_NODE_INIT(tfc_req_mask, "req_mask",
				    TYPE_TFC_REQ_MASK);
	wla_dbg_sysfs_sub_entry_node_add(tfc_req_mask.name,
					WLA_DBG_SYS_FS_MODE,
					&tfc_req_mask.op,
					&wla_dbg_tfc,
					&tfc_req_mask.handle);

	WLA_TFC_NODE_INIT(tfc_urg_mask, "urg_mask",
				    TYPE_TFC_URG_MASK);
	wla_dbg_sysfs_sub_entry_node_add(tfc_urg_mask.name,
					WLA_DBG_SYS_FS_MODE,
					&tfc_urg_mask.op,
					&wla_dbg_tfc,
					&tfc_urg_mask.handle);

	wla_dbg_sysfs_sub_entry_add("monitor", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_entry_dbg_monitor);

	WLA_MON_NODE_INIT(start, "start",
				    TYPE_START);
	wla_dbg_sysfs_sub_entry_node_add(start.name,
					WLA_DBG_SYS_FS_MODE,
					&start.op,
					&wla_entry_dbg_monitor,
					&start.handle);

	WLA_MON_NODE_INIT(ch_cnt, "ch_cnt",
				    TYPE_CH_CNT);
	wla_dbg_sysfs_sub_entry_node_add(ch_cnt.name,
					WLA_DBG_SYS_FS_MODE,
					&ch_cnt.op,
					&wla_entry_dbg_monitor,
					&ch_cnt.handle);

	WLA_MON_NODE_INIT(mon_sel, "mon_sel",
				    TYPE_MON_SEL);
	wla_dbg_sysfs_sub_entry_node_add(mon_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&mon_sel.op,
					&wla_entry_dbg_monitor,
					&mon_sel.handle);

	WLA_MON_NODE_INIT(ddr_sta_mux, "ddr_sta_mux",
				    TYPE_DDR_STA_MUX);
	wla_dbg_sysfs_sub_entry_node_add(ddr_sta_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&ddr_sta_mux.op,
					&wla_entry_dbg_monitor,
					&ddr_sta_mux.handle);

	WLA_MON_NODE_INIT(pt_sta_mux, "pt_sta_mux",
				    TYPE_PT_STA_MUX);
	wla_dbg_sysfs_sub_entry_node_add(pt_sta_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&pt_sta_mux.op,
					&wla_entry_dbg_monitor,
					&pt_sta_mux.handle);

	WLA_MON_NODE_INIT(vlc_mux, "vlc_mux",
				    TYPE_VLC_MUX);
	wla_dbg_sysfs_sub_entry_node_add(vlc_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&vlc_mux.op,
					&wla_entry_dbg_monitor,
					&vlc_mux.handle);

	WLA_MON_NODE_INIT(tfc_mux, "tfc_mux",
				    TYPE_TFC_MUX);
	wla_dbg_sysfs_sub_entry_node_add(tfc_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&tfc_mux.op,
					&wla_entry_dbg_monitor,
					&tfc_mux.handle);

	WLA_MON_NODE_INIT(pmsr_ocla_mux, "pmsr_ocla_mux",
				    TYPE_PMSR_OCLA_MUX);
	wla_dbg_sysfs_sub_entry_node_add(pmsr_ocla_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&pmsr_ocla_mux.op,
					&wla_entry_dbg_monitor,
					&pmsr_ocla_mux.handle);

	wla_dbg_sysfs_sub_entry_add("dbg_latch", WLA_DBG_SYS_FS_MODE,
				NULL, &wla_dbg_latch);

	WLA_DBG_LATCH_NODE_INIT(lat_sel, "lat_sel",
				    TYPE_LAT_SEL);
	wla_dbg_sysfs_sub_entry_node_add(lat_sel.name,
					WLA_DBG_SYS_FS_MODE,
					&lat_sel.op,
					&wla_dbg_latch,
					&lat_sel.handle);

	WLA_DBG_LATCH_NODE_INIT(lat_sta_mux, "lat_sta_mux",
				    TYPE_LAT_STA_MUX);
	wla_dbg_sysfs_sub_entry_node_add(lat_sta_mux.name,
					WLA_DBG_SYS_FS_MODE,
					&lat_sta_mux.op,
					&wla_dbg_latch,
					&lat_sta_mux.handle);
	return 0;
}

static void __exit wla_fs_exit(void)
{
}

module_init(wla_fs_init);
module_exit(wla_fs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK wla debug module - v1");
MODULE_AUTHOR("MediaTek Inc.");
