// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"

#include "mtk_disp_vidle.h"

#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
#include <linux/interconnect.h>
#include "mtk_disp_bdg.h"
extern u32 *disp_perfs;
#endif
#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif

#include <linux/module.h>
int debug_vidle_bw;
module_param(debug_vidle_bw, int, 0644);

int debug_channel_bw[4];
module_param_array(debug_channel_bw, int, NULL, 0644);

#define CRTC_NUM		4
static struct drm_crtc *dev_crtc;
/* add for mm qos */
static u8 vdisp_opp = U8_MAX;
static struct regulator *mm_freq_request;
static unsigned long *g_freq_steps;
static unsigned int lp_freq;
static int g_freq_level[CRTC_NUM] = {-1, -1, -1, -1};
static bool g_freq_lp[CRTC_NUM] = {false, false, false, false};
static long g_freq;
static int step_size = 1;

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
struct hrt_mmclk_request {
	int layer_num;
	int volt;
};
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_bdg_mt6768[] = {
	{20, 700000},
	{40, 700000},
	{45, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_bdg_fhdp_mt6768[] = {
	{20, 700000},
	{30, 700000},
	{40, 800000},
};

/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_mt6768[] = {
	{35, 650000},
	{60, 700000},
	{70, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_fhdp_mt6768[] = {
	{30, 650000},
	{50, 700000},
	{60, 800000},
};

struct hrt_mmclk_request hrt_req_level_mt6761[] = {
	{40, 650000},
	{60, 700000},
	{70, 800000},
};

//LPDDR3
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr3_hd_mt6765[] = {
	{75, 650000},
	{75, 700000},
	{75, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr3_fhd_mt6765[] = {
	{35, 650000},
	{35, 700000},
	{35, 800000},
};

//LPDDR4
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_hd_mt6765[] = {
	{75, 650000},
	{135, 700000},
	{155, 800000},
};

/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_fhd_mt6765[] = {
	{35, 650000},
	{60, 700000},
	{70, 800000},
};

//LPDDR4 and high fps
/* aspect ratio <= 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_hd_hfps_mt6765[] = {
	{50, 650000},
	{90, 700000},
	{100, 800000},
};
/* aspect ratio > 18 : 9 */
struct hrt_mmclk_request hrt_req_level_ddr4_fhd_hfps_mt6765[] = {
	{20, 650000},/*23*/
	{40, 700000},/*40*/
	{45, 800000},/*47*/
};
#endif
void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	/* mtk_dump_comp_str return shorter comp name, add prefix to match icc name in dts */
	len = snprintf(buf, buf_len, "DDP_COMPONENT_%s_%s", mtk_dump_comp_str(comp), qos_event);
	if (!(len > -1 && len < buf_len))
		DDPINFO("%s: snprintf return error", __func__);
}

int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode)
{
	switch (comp_id) {
	case DDP_COMPONENT_OVL0:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_BW;
	case DDP_COMPONENT_OVL1:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_BW;
	case DDP_COMPONENT_OVL0_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_BW;
	case DDP_COMPONENT_OVL1_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_BW;
	case DDP_COMPONENT_OVL2_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL2_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL2_2L_BW;
	case DDP_COMPONENT_OVL3_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL3_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL3_2L_BW;
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_NWCG_BW;
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_NWCG_BW;
	case DDP_COMPONENT_RDMA0:
		return DISP_PMQOS_RDMA0_BW;
	case DDP_COMPONENT_RDMA1:
		return DISP_PMQOS_RDMA1_BW;
	case DDP_COMPONENT_RDMA2:
		return DISP_PMQOS_RDMA2_BW;
	case DDP_COMPONENT_WDMA0:
		return DISP_PMQOS_WDMA0_BW;
	case DDP_COMPONENT_WDMA1:
		return DISP_PMQOS_WDMA1_BW;
	case DDP_COMPONENT_OVLSYS_WDMA2:
		return DISP_PMQOS_OVLSYS_WDMA2_BW;
	default:
		DDPPR_ERR("%s, unknown comp %d\n", __func__, comp_id);
		break;
	}

	return -EINVAL;
}

int __mtk_disp_set_module_srt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, unsigned int peak_bw, unsigned int bw_mode,
				bool real_srt_ostdl)
{
	DDPDBG("%s set %s bw = %u peak %u, srt_ostdl %d\n", __func__,
			mtk_dump_comp_str_id(comp_id), bandwidth, peak_bw, real_srt_ostdl);
	if (real_srt_ostdl != true)
		bandwidth = bandwidth * 133 / 100;

	mtk_icc_set_bw(request, MBps_to_icc(bandwidth), MBps_to_icc(peak_bw));

	DRM_MMP_MARK(pmqos, comp_id, bandwidth);

	return 0;
}

void __mtk_disp_set_module_hrt(struct icc_path *request,
				unsigned int bandwidth, bool respective_ostdl)
{
	if (bandwidth > 0 && respective_ostdl != true)
		mtk_icc_set_bw(request, 0, MTK_MMQOS_MAX_BW);
	else
		mtk_icc_set_bw(request, 0, MBps_to_icc(bandwidth));
}

static bool mtk_disp_check_segment(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;
	int hact = 0;
	int vact = 0;
	int vrefresh = 0;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s, mtk_crtc is NULL\n", __func__);
		return ret;
	}

	if (IS_ERR_OR_NULL(priv)) {
		DDPPR_ERR("%s, private is NULL\n", __func__);
		return ret;
	}

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	if (priv->data->mmsys_id == MMSYS_MT6897 && !priv->is_tablet) {
		switch (priv->seg_id) {
		case 1:
			if (hact >= 1440 && vrefresh > 120)
				ret = false;
			break;
		case 2:
			if (hact >= 1440 && vrefresh > 144)
				ret = false;
			break;
		default:
			ret = true;
			break;
		}
	}

/*
 *	DDPMSG("%s, segment:%d, mode(%d, %d, %d)\n",
 *			__func__, priv->seg_id, hact, vact, vrefresh);
 */

	if (ret == false)
		DDPPR_ERR("%s, check sement fail: segment:%d, mode(%d, %d, %d)\n",
			__func__, priv->seg_id, hact, vact, vrefresh);

	return ret;
}

static unsigned int mtk_disp_getMaxBW(unsigned int arr[], int size,
						unsigned int total_bw)
{
	unsigned int maxVal = arr[0];

	for (int i = 1; i < size; i++) {
		if (arr[i] > maxVal)
			maxVal = arr[i];
	}

	DDPINFO("%s maxVal = %d, total_bw = %d\n",__func__, maxVal, total_bw);

	if (maxVal > total_bw)
		return total_bw;
	else
		return maxVal;
}

static unsigned int mtk_disp_larb_hrt_bw_MT6989(struct mtk_drm_crtc *mtk_crtc,
						unsigned int total_bw, unsigned int bw_base)
{
	int i = 0;
	int max_sub_comm = 4; // 6989 sub common num
	int max_ovl_phy_layer = 12; // 6989 phy ovl layer num
	unsigned int subcomm_bw_sum[4] = {0};
	/* sub_comm0: layer0 + layer4 + layer9
	 * sub_comm1: layer1 + layer5 + layer8
	 * sub_comm2: layer2 + layer7 + layer11
	 * sub_comm3: layer3 + layer6 + layer10
	 */
	for (i = 0; i < max_ovl_phy_layer; i++) {
		if (mtk_crtc->usage_ovl_fmt[i]) {
			if (i == 0 || i == 4 || i == 9)
				subcomm_bw_sum[0] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 1 || i == 5 || i == 8)
				subcomm_bw_sum[1] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 2 || i == 7 || i == 11)
				subcomm_bw_sum[2] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 3 || i == 6 || i == 10)
				subcomm_bw_sum[3] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
		}
	}

	return mtk_disp_getMaxBW(subcomm_bw_sum, max_sub_comm, total_bw);
}

static unsigned int mtk_disp_larb_hrt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc,
						unsigned int total_bw, unsigned int bw_base)
{
	int i = 0;
	int max_sub_comm = 4; // 6991 sub common num
	int max_ovl_phy_layer = 12; // 6991 phy ovl layer num
	unsigned int subcomm_bw_sum[4] = {0};
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	int oddmr_hrt = 0;

	/* sub_comm0: exdma2(0) + exdma7(5) + 1_exdma5(11) + (1_exdma8)
	 * sub_comm1: exdma3(1) + exdma6(4) + 1_exdma4(10) + (1_exdma9)
	 * sub_comm2: exdma4(2) + exdma9(7) + 1_exdma3(9) + (1_exdma6)
	 * sub_comm3: exdma5(3) + exdma8(6) + 1_exdma2(8) + (1_exdma7)
	 */
	for (i = 0; i < max_ovl_phy_layer; i++) {
		if (mtk_crtc->usage_ovl_fmt[i]) {
			if (i == 0 || i == 5 || i == 11) {
				subcomm_bw_sum[0] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
				if (priv->last_hrt_channel_bw_sum[crtc_idx][0] != subcomm_bw_sum[0])
					priv->hrt_channel_bw_sum[crtc_idx][0] = subcomm_bw_sum[0];
			} else if (i == 1 || i == 4 || i == 10) {
				subcomm_bw_sum[1] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
				if (priv->last_hrt_channel_bw_sum[crtc_idx][1] != subcomm_bw_sum[1])
					priv->hrt_channel_bw_sum[crtc_idx][1] = subcomm_bw_sum[1];
			} else if (i == 2 || i == 7 || i == 9) {
				subcomm_bw_sum[2] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
				if (priv->last_hrt_channel_bw_sum[crtc_idx][2] != subcomm_bw_sum[2])
					priv->hrt_channel_bw_sum[crtc_idx][2] = subcomm_bw_sum[2];
			} else if (i == 3 || i == 6 || i == 8) {
				subcomm_bw_sum[3] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
				if (priv->last_hrt_channel_bw_sum[crtc_idx][3] != subcomm_bw_sum[3])
					priv->hrt_channel_bw_sum[crtc_idx][3] = subcomm_bw_sum[3];
			}
		}
	}

	if (priv->data->mmsys_id == MMSYS_MT6991) {
		mtk_oddmr_hrt_cal_notify(&oddmr_hrt);
		subcomm_bw_sum[2] += bw_base * oddmr_hrt / 400;
	}

	return mtk_disp_getMaxBW(subcomm_bw_sum, max_sub_comm, total_bw);
}

static void mtk_disp_channel_hrt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	unsigned int channel_sum = 0;

	for (i = 0; i < BW_CHANNEL_NR; i++) {
		for (j = 0; j < MAX_CRTC; j++)
			channel_sum += priv->hrt_channel_bw_sum[j][i];
		if (debug_channel_bw[i])
			channel_sum = debug_channel_bw[i];
		mtk_vidle_channel_bw_set(channel_sum, 2 + (i * 4)); //2, 6, 10, 14
		DDPINFO("%s, total hrt bw %d\n", __func__, channel_sum);

		priv->last_hrt_channel_bw_sum[crtc_idx][i] = priv->hrt_channel_bw_sum[crtc_idx][i];
		channel_sum = 0;
	}
}

static void mtk_disp_clear_channel_hrt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);

	for (i = 0; i < BW_CHANNEL_NR; i++)
		priv->hrt_channel_bw_sum[crtc_idx][i] = 0;
}

static void mtk_disp_channel_srt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i,j;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int channel_sum = 0;
	char dbg_msg[512] = {0};
	int written = 0;


	written = scnprintf(dbg_msg, 512, "%s = ", __func__);
	for (i = 0; i < BW_CHANNEL_NR; i++) {
		for (j = 0; j < MAX_CRTC; j++)
			channel_sum += priv->srt_channel_bw_sum[j][i];
		mtk_vidle_channel_bw_set(channel_sum, (i * 4)); //0, 4, 8, 12

		written += scnprintf(dbg_msg + written, 512 - written, "[%d]", channel_sum);

		channel_sum = 0;
	}
	DDPINFO("%s\n", dbg_msg);

}

static void mtk_disp_clear_channel_srt_bw_MT6991(struct mtk_drm_crtc *mtk_crtc)
{
	int i;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);

	for (i = 0; i < BW_CHANNEL_NR; i++)
		priv->srt_channel_bw_sum[crtc_idx][i] = 0;
}

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
void mtk_disp_hrt_mmclk_request_mt6768(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct hrt_mmclk_request *req_level;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPINFO("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	if (is_bdg_supported()) {
		if (mtk_crtc->base.mode.vdisplay / mtk_crtc->base.mode.hdisplay > 18 / 9)
			req_level = hrt_req_level_bdg_fhdp_mt6768;
		else
			req_level = hrt_req_level_bdg_mt6768;
	} else {
		if (mtk_crtc->base.mode.vdisplay / mtk_crtc->base.mode.hdisplay > 18 / 9)
			req_level = hrt_req_level_fhdp_mt6768;
		else
			req_level = hrt_req_level_mt6768;
	}

	if (layer_num <= req_level[0].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if (layer_num > req_level[0].layer_num && layer_num <= req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}

void mtk_disp_hrt_mmclk_request_mt6761(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct hrt_mmclk_request *req_level = hrt_req_level_mt6761;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPMSG("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	if (layer_num <= req_level[0].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if (layer_num > req_level[0].layer_num && layer_num <= req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}

void mtk_disp_hrt_mmclk_request_mt6765(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	int layer_num;
	int ret;
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	unsigned int ddr_type;
#endif
	unsigned long long bw_base;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	struct hrt_mmclk_request *req_level = hrt_req_level_ddr3_fhd_mt6765;
#else
	struct hrt_mmclk_request *req_level = hrt_req_level_ddr4_fhd_mt6765;
#endif
	struct mtk_ddp_comp *output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	struct drm_display_mode *mode = NULL;
	unsigned int max_fps = 0;
	bool is_tall_aspect_ratio = ((mtk_crtc->base.mode.vdisplay*100) /
					mtk_crtc->base.mode.hdisplay) > 200 /*18:9*/;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (bw_base != 0)
		layer_num = bw * 10 / bw_base;
	else {
		DDPINFO("%s-error: frame_bw is zero, skip request mmclk\n", __func__);
		return;
	}

	mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_MODE_BY_MAX_VREFRESH, &mode);
	if (mode)
		max_fps = drm_mode_vrefresh(mode);
	if (max_fps >= 90) {
		if (is_tall_aspect_ratio ||
			(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
			DDPINFO("%s DDR4, fhd@90, h:%d", __func__, mtk_crtc->base.mode.hdisplay);
			req_level = hrt_req_level_ddr4_fhd_hfps_mt6765;
		} else {
			DDPINFO("%s DDR4, hd@90, h:%d", __func__, mtk_crtc->base.mode.hdisplay);
			req_level = hrt_req_level_ddr4_hd_hfps_mt6765;
		}
	} else { /*(max_fps < 90)*/
#if IS_ENABLED(CONFIG_MTK_DRAMC)
		ddr_type = mtk_dramc_get_ddr_type();
		if (ddr_type == TYPE_LPDDR4 || ddr_type == TYPE_LPDDR4X) {
			if (is_tall_aspect_ratio ||
				(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
				DDPINFO("%s DDR4, fhd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr4_fhd_mt6765;
			} else {
				DDPINFO("%s DDR4, hd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr4_hd_mt6765;
			}
		} else { /*ddr3*/
			if (is_tall_aspect_ratio ||
				(mtk_crtc->base.mode.hdisplay > 800)/*fhdp*/) {
				DDPINFO("%s DDR3, fhd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr3_fhd_mt6765;
			} else {
				DDPINFO("%s DDR3, hd, h:%d",
					__func__, mtk_crtc->base.mode.hdisplay);
				req_level = hrt_req_level_ddr3_hd_mt6765;
			}
		}
#endif
	} /*(max_fps >= 90)*/

	if (layer_num <= req_level[0].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL2]);
		ret = regulator_set_voltage(mm_freq_request, req_level[0].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[0].volt);
	} else if (layer_num > req_level[0].layer_num && layer_num <= req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL1]);
		ret = regulator_set_voltage(mm_freq_request, req_level[1].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[1].volt);
	} else if (layer_num > req_level[1].layer_num) {
		icc_set_bw(priv->hrt_bw_request, 0, disp_perfs[HRT_LEVEL_LEVEL0]);
		ret = regulator_set_voltage(mm_freq_request, req_level[2].volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
		DDPMSG("%s layer_num = %d, volt = %d\n", __func__, layer_num, req_level[2].volt);
	}
}
#endif

unsigned int mtk_disp_get_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int tmp = NO_PENDING_HRT, bw_base = 0;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (priv->data->mmsys_id == MMSYS_MT6989) {
		if (bw_base != MAX_MMCLK)
			tmp = mtk_disp_larb_hrt_bw_MT6989(mtk_crtc, MAX_MMCLK, bw_base);
		else
			tmp = bw_base;
	} else if (priv->data->mmsys_id == MMSYS_MT6991) {
		if (bw_base != MAX_MMCLK)
			tmp = mtk_disp_larb_hrt_bw_MT6991(mtk_crtc, MAX_MMCLK, bw_base);
		else
			tmp = bw_base;
	}
	return tmp;
}

bool mtk_disp_check_channel_hrt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	int i = 0;
	bool check_flag = false;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);

	for (i = 0; i < BW_CHANNEL_NR; i++)
		if (priv->last_hrt_channel_bw_sum[crtc_idx][i] !=
				priv->hrt_channel_bw_sum[crtc_idx][i])
			check_flag = true;
	return check_flag;
}


void mtk_disp_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_disp_channel_srt_bw_MT6991(mtk_crtc);

}

void mtk_disp_clear_channel_srt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_disp_clear_channel_srt_bw_MT6991(mtk_crtc);
}

void mtk_disp_set_module_hrt(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_ddp_comp *comp;
	unsigned int bw_base  = 0;
	int i, j, ret = 0;

	if (mtk_crtc == NULL)
		return;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	for (i = 0; i < DDP_PATH_NR; i++) {
		if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
			continue;
		if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
			continue;
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
						   &bw_base);
		}
		if (!mtk_crtc->is_dual_pipe)
			continue;
		for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i)
			mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
					&bw_base);
	}
}

void mtk_disp_clr_module_hrt(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_ddp_comp *comp;
	int i, j, ret = 0;

	if (mtk_crtc == NULL)
		return;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return;

	for (i = 0; i < DDP_PATH_NR; i++) {
		if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
			continue;
		if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
			continue;
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_CLR_HRT_BW, NULL);
		}
		if (!mtk_crtc->is_dual_pipe)
			continue;
		for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i)
			mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_CLR_HRT_BW, NULL);
	}
}

int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int tmp, total = 0, tmp1 = 0, bw_base  = 0;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	int i, ret = 0, ovl_num = 0;

	tmp = bw;

	if (mtk_crtc == NULL)
		return 0;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return 0;

	if (priv->data->mmsys_id == MMSYS_MT6768 ||
		priv->data->mmsys_id == MMSYS_MT6761 ||
		priv->data->mmsys_id == MMSYS_MT6765) {
		DDPMSG("%s: no need to set module hrt bw for legacy!\n", __func__);
	}
	if (ret == RDMA_REQ_HRT)
		tmp = mtk_drm_primary_frame_bw(crtc);
	else if (ret == MDP_RDMA_REQ_HRT)
		return 0;

	/* skip same HRT BW */
	if (priv->req_hrt[crtc_idx] == tmp)
		return 0;

	priv->req_hrt[crtc_idx] = tmp;

	for (i = 0; i < MAX_CRTC; ++i)
		total += priv->req_hrt[i];

	if (bw == MAX_MMCLK) {
		DDPMSG("%s,total:%d->65535\n", __func__, total);
		total = 65535;
	}

#ifdef CONFIG_MTK_FB_MMDVFS_SUPPORT
	if (priv->data->mmsys_id == MMSYS_MT6768)
		mtk_disp_hrt_mmclk_request_mt6768(mtk_crtc, tmp);
	else if (priv->data->mmsys_id == MMSYS_MT6761)
		mtk_disp_hrt_mmclk_request_mt6761(mtk_crtc, tmp);
	else if (priv->data->mmsys_id == MMSYS_MT6765)
		mtk_disp_hrt_mmclk_request_mt6765(mtk_crtc, tmp);
#else
	if ((priv->data->mmsys_id == MMSYS_MT6897) &&
		(mtk_disp_check_segment(mtk_crtc, priv) == false))
		mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(1));
	else
		mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(total));

	if (debug_vidle_bw)
		total = debug_vidle_bw;

	mtk_vidle_hrt_bw_set(total);
#endif
	DRM_MMP_MARK(hrt_bw, 0, tmp);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HRT_BY_LARB)) {

		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DPTX) {
			tmp = tmp / (mtk_crtc->is_dual_pipe + 1);
			mtk_icc_set_bw(priv->dp_hrt_by_larb, 0, MBps_to_icc(tmp));
			DDPINFO("%s, CRTC%d(DP) HRT total=%u larb bw=%u dual=%d\n",
				__func__, crtc_idx, total, tmp, mtk_crtc->is_dual_pipe);
		} else if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI &&
				(priv->data->mmsys_id != MMSYS_MT6989) &&
				(priv->data->mmsys_id != MMSYS_MT6991)) {
			if (total > 0) {
				bw_base = mtk_drm_primary_frame_bw(crtc);
				ovl_num = bw_base > 0 ? total / bw_base : 0;
				tmp1 = ((bw_base / 2) > total) ? total : (ovl_num < 3) ?
					(bw_base / 2) : (ovl_num < 5) ?
					bw_base : (bw_base * 3 / 2);
			}

			if ((priv->data->mmsys_id == MMSYS_MT6897) &&
				(mtk_disp_check_segment(mtk_crtc, priv) == false))
				mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(1));
			else
				mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(tmp1));

			mtk_vidle_dvfs_bw_set(tmp1);
			mtk_crtc->qos_ctx->last_larb_hrt_req = tmp1;
			DDPINFO("%s, CRTC%d HRT bw=%u total=%u larb bw=%u ovl_num=%d bw_base=%d\n",
				__func__, crtc_idx, tmp, total, tmp1, ovl_num, bw_base);
		}
	} else
		DDPINFO("set CRTC %d HRT bw %u %u\n", crtc_idx, tmp, total);

	return ret;
}

int mtk_disp_set_per_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int total = 0xFFFFFFFF, tmp1 = 0, bw_base  = 0;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	int ret = 0;

	if (mtk_crtc == NULL)
		return 0;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return 0;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_HRT_BY_LARB))
		return 0;

	if (bw == 0) {
		bw_base = bw;
		total = bw;
	} else
		bw_base = mtk_drm_primary_frame_bw(crtc);

	comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI) {
		if (total > 0) {
			if (priv->data->mmsys_id == MMSYS_MT6989) {
				if (bw != MAX_MMCLK)
					tmp1 = mtk_disp_larb_hrt_bw_MT6989(mtk_crtc, total, bw_base);
				else
					tmp1 = bw;
			} else if (priv->data->mmsys_id == MMSYS_MT6991) {
				if (bw != MAX_MMCLK)
					tmp1 = mtk_disp_larb_hrt_bw_MT6991(mtk_crtc, total, bw_base);
				else
					tmp1 = bw;
			}
		} else {
			tmp1 = bw;
			mtk_disp_clear_channel_hrt_bw_MT6991(mtk_crtc);
		}

		mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(tmp1));

		mtk_vidle_dvfs_bw_set(tmp1);
		if (priv->data->mmsys_id == MMSYS_MT6991)
			mtk_disp_channel_hrt_bw_MT6991(mtk_crtc);
		DDPINFO("%s, CRTC%d larb bw=%u bw_base=%d\n",
			__func__, crtc_idx, tmp1, bw_base);
	}

	return 0;
}

void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode;
	unsigned int bw = 0;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);
	mode = &crtc->state->adjusted_mode;

	bw = _layering_get_frame_bw(crtc, mode);
	mtk_disp_set_hrt_bw(mtk_crtc, bw);
	DDPINFO("%s:pan_disp_set_hrt_bw: %u\n", caller, bw);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HRT_BY_LARB) &&
		(priv->data->mmsys_id == MMSYS_MT6989 ||
		priv->data->mmsys_id == MMSYS_MT6991)) {

		/* FIXME: this value is zero when booting, will be assigned in exdma_layer_config */
		mtk_crtc->usage_ovl_fmt[1] = 4;

		mtk_disp_set_per_larb_hrt_bw(mtk_crtc, bw);
	}
}

void mtk_disp_hrt_repaint_blocking(const unsigned int hrt_idx)
{
	int i, ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);

	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev_crtc->dev);
	for (i = 0; i < 5; ++i) {
		ret = wait_event_timeout(
			mtk_crtc->qos_ctx->hrt_cond_wq,
			atomic_read(&mtk_crtc->qos_ctx->hrt_cond_sig),
			HZ / 5);
		if (ret == 0)
			DDPINFO("wait repaint timeout %d\n", i);
		atomic_set(&mtk_crtc->qos_ctx->hrt_cond_sig, 0);
		if (atomic_read(&mtk_crtc->qos_ctx->last_hrt_idx) >= hrt_idx)
			break;
	}
}

/* force report all display's mmqos BW include SRT & HRT */
void mtk_disp_mmqos_bw_repaint(struct mtk_drm_private *priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	unsigned int i, j, k, c, tmp = 1, flag = DISP_BW_FORCE_UPDATE;
	int ret = 0;
	bool is_hrt;

	for (c = 0 ; c < MAX_CRTC ; ++c) {
		crtc = priv->crtc[c];
		if (crtc == NULL)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);

		DDP_MUTEX_LOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);

		if (!(mtk_crtc->enabled)) {
			DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
			continue;
		}

		for (k = 0; k < DDP_PATH_NR; k++) {
			is_hrt = (mtk_crtc->ddp_mode < DDP_MODE_NR) ?
				mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[k] : false;
			tmp = mtk_drm_primary_frame_bw(crtc);
			for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, k) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
			if (!mtk_crtc->is_dual_pipe)
				continue;
			for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
		}

		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
	}
}

int mtk_disp_hrt_cond_change_cb(struct notifier_block *nb, unsigned long value,
				void *v)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);
	unsigned int hrt_idx;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* No need to repaint when display suspend */
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	switch (value) {
	case BW_THROTTLE_START: /* CAM on */
		DDPMSG("DISP BW Throttle start\n");
		/* TODO: concider memory session */
		DDPINFO("CAM trigger repaint\n");
		hrt_idx = _layering_rule_get_hrt_idx(drm_crtc_index(dev_crtc));
		hrt_idx++;
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mtk_disp_hrt_repaint_blocking(hrt_idx);
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		break;
	case BW_THROTTLE_END: /* CAM off */
		DDPMSG("DISP BW Throttle end\n");
		/* TODO: switch DC */
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		/* bw repaint might hold all crtc's mutex, need unlock current mutex first */
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		break;
	default:
		break;
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

struct notifier_block pmqos_hrt_notifier = {
	.notifier_call = mtk_disp_hrt_cond_change_cb,
};

int mtk_disp_hrt_bw_dbg(void)
{
	mtk_disp_hrt_cond_change_cb(NULL, BW_THROTTLE_START, NULL);

	return 0;
}

int mtk_disp_hrt_cond_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(dev_crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s:mtk_crtc is NULL\n", __func__);
		return -EINVAL;
	}

	priv = mtk_crtc->base.dev->dev_private;

	mtk_crtc->qos_ctx = vmalloc(sizeof(struct mtk_drm_qos_ctx));
	if (mtk_crtc->qos_ctx == NULL) {
		DDPPR_ERR("%s:allocate qos_ctx failed\n", __func__);
		return -ENOMEM;
	}
	atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, 0);
	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_mmclk_req_idx = 0;
	mtk_crtc->qos_ctx->last_larb_hrt_req = 0;

	if (drm_crtc_index(crtc) == 0 && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_mmqos_register_bw_throttle_notifier(&pmqos_hrt_notifier);

	return 0;
}

static void mtk_drm_mmdvfs_get_avail_freq(struct device *dev)
{
	int i = 0;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int ret;

	step_size = dev_pm_opp_get_opp_count(dev);
	g_freq_steps = kcalloc(step_size, sizeof(unsigned long), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_freq_steps[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	ret = of_property_read_u32(dev->of_node, "lp-mmclk-freq", &lp_freq);
	DDPINFO("%s lp_freq = %d\n", __func__, lp_freq);
}

void mtk_drm_mmdvfs_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	int ret = 0;

	dev_pm_opp_of_add_table(dev);
	mtk_drm_mmdvfs_get_avail_freq(dev);

	/* support DPC and VDISP */
	ret = of_property_read_u8(node, "vdisp-dvfs-opp", &vdisp_opp);
	if (ret == 0) {
		mm_freq_request = devm_regulator_get_optional(dev, "dis1-shutdown");
		if (mm_freq_request == NULL)
			DDPMSG("%s use vdisp opp(%u)\n", __func__, vdisp_opp);
		else if (IS_ERR(mm_freq_request))
			mm_freq_request = NULL;
		else
			DDPMSG("%s use vdisp but regulator flow\n", __func__);
		return;
	}

	/* MMDVFS V2 */
	DDPINFO("%s, try to use MMDVFS V2\n", __func__);
	mm_freq_request = devm_regulator_get_optional(dev, "mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(mm_freq_request))
		DDPPR_ERR("%s, get mmdvfs-dvfsrc-vcore failed\n", __func__);
}

unsigned int mtk_drm_get_mmclk_step_size(void)
{
	return step_size;
}

void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, bool lp_mode,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret, idx, i;
	int final_lp_mode = true;
	int final_level = -1;
	int cnt = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	idx = drm_crtc_index(crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s invalid mtk_crtc\n", __func__);
		return;
	}

	/* memory session do not use */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (IS_ERR_OR_NULL(output_comp) ||
		mtk_ddp_comp_get_type(output_comp->id) == MTK_DISP_WDMA) {
		DDPINFO("crtc%d not support set mmclk\n", idx);
		return;
	}

	DDPINFO("%s[%d] g_freq_level[idx=%d]: %d, g_freq_lp[idx=%d]: %d\n",
		__func__, __LINE__, idx, level, idx, lp_mode);

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level[idx] && lp_mode == g_freq_lp[idx])
		return;

	g_freq_lp[idx] = lp_mode;
	g_freq_level[idx] = level;

	for (i = 0; i < CRTC_NUM; i++) {
		if (g_freq_level[i] == -1)
			cnt++;

		if (g_freq_level[i] != -1 && !g_freq_lp[i])
			final_lp_mode = false;

		if (g_freq_level[i] > final_level)
			final_level = g_freq_level[i];
	}

	if (cnt == CRTC_NUM)
		final_lp_mode = false;

	if (final_level >= 0)
		freq = g_freq_steps[final_level];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d] final_level(freq=%d, %lu) final_lp_mode:%d\n",
		__func__, __LINE__, final_level, freq, final_lp_mode);

	if ((vdisp_opp != U8_MAX) && (mm_freq_request == NULL)) {
		if (final_level >= 0)
			vdisp_opp = final_level;
		mtk_vidle_dvfs_set(vdisp_opp);
		return;
	}

	if (mm_freq_request) {
		if (vdisp_opp == U8_MAX) /* not support for vdisp platform */
			mmdvfs_set_lp_mode(final_lp_mode);

		opp = dev_pm_opp_find_freq_ceil(crtc->dev->dev, &freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
		ret = regulator_set_voltage(mm_freq_request, volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
	}
}

void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc,
	unsigned int pixclk, const char *caller)
{
	int i;
	unsigned long freq = pixclk * 1000000;

	g_freq = freq;

	if (freq > g_freq_steps[step_size - 1]) {
		DDPPR_ERR("%s:pixleclk (%lu) is to big for mmclk (%lu)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, false, caller);
		return;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, false, caller);
		return;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, false, caller);
			break;
		}
		if (i == 0) {
			if (!lp_freq || (lp_freq && (freq > lp_freq)))
				mtk_drm_set_mmclk(crtc, 0, false, caller);
			else
				mtk_drm_set_mmclk(crtc, 0, true, caller);
		}
	}
}

unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller)
{
	return g_freq;
}

unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller)
{
	int idx;
	unsigned long freq;

	if (!crtc || !g_freq_steps)
		return 0;

	idx = drm_crtc_index(crtc);

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d]g_freq_level[idx=%d]: %d (freq=%lu)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

	return freq;
}

