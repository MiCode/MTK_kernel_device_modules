// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * Copyright (c) 2022 BayLibre
 */

#include "mtk_drm_edp_phy_autotest.h"

static u32 mtk_edp_autotest_read(struct dp_cts_auto_req *mtk_edp_test, u32 offset)
{
	u32 read_val;
	int ret;

	ret = regmap_read(mtk_edp_test->regs, offset, &read_val);
	if (ret) {
		pr_info("%s Failed to read register 0x%x: %d\n",
				EDP_PHY_AUTOTEST_DEBUG, offset, ret);
		return 0;
	}

	return read_val;
}

static int mtk_edp_autotest_write(struct dp_cts_auto_req *mtk_edp_test,
			u32 offset, u32 val)
{
	int ret = regmap_write(mtk_edp_test->regs, offset, val);

	if (ret)
		pr_info("%s Failed to write register 0x%8x with value 0x%x\n",
				EDP_PHY_AUTOTEST_DEBUG, offset, val);

	return ret;
}

static int mtk_edp_autotest_update_bits(struct dp_cts_auto_req *mtk_edp_test,
			u32 offset, u32 val, u32 mask)
{
	int ret = regmap_update_bits(mtk_edp_test->regs, offset, mask, val);

	if (ret)
		pr_info("%s Failed to update register 0x%04x with value 0x%08x, mask 0x%x\n",
			EDP_PHY_AUTOTEST_DEBUG, offset, val, mask);

	return ret;
}

static void mtk_edp_autotest_bulk_16bit_write(struct dp_cts_auto_req *mtk_edp_test,
			u32 offset, u8 *buf, size_t length)
{
	int i;

	/* 2 bytes per register */
	for (i = 0; i < length; i += 2) {
		u32 val = buf[i] | (i + 1 < length ? buf[i + 1] << 8 : 0);

		if (mtk_edp_autotest_write(mtk_edp_test, offset + i * 2, val))
			return;
	}
}

static u32 mtk_edp_phy_autotest_read(struct dp_cts_auto_req *mtk_edp_test, u32 offset)
{
	u32 read_val;
	int ret;

	ret = regmap_read(mtk_edp_test->phyd_regs, offset, &read_val);
	if (ret) {
		pr_info("%s Failed to read phy register 0x%x: %d\n",
				EDP_PHY_AUTOTEST_DEBUG, offset, ret);
		return 0;
	}

	return read_val;
}

static int mtk_edp_phy_autotest_write(struct dp_cts_auto_req *mtk_edp_test, u32 offset, u32 val)
{
	int ret = regmap_write(mtk_edp_test->phyd_regs, offset, val);

	if (ret)
		pr_info("%s Failed to write phy register 0x%8x with value 0x%x\n",
				EDP_PHY_AUTOTEST_DEBUG, offset, val);

	return ret;
}

static int mtk_edp_phy_autotest_update_bits(struct dp_cts_auto_req *mtk_edp_test, u32 offset,
			      u32 val, u32 mask)
{
	int ret = regmap_update_bits(mtk_edp_test->phyd_regs, offset, mask, val);

	if (ret)
		pr_info("%s Failed to update phy register 0x%04x with value 0x%08x, mask 0x%x\n",
			EDP_PHY_AUTOTEST_DEBUG, offset, val, mask);

	return ret;
}

static void mtk_edp_aux_write_access(struct dp_cts_auto_req *mtk_edp_test, u32 dpcd_addr,
					u8 *buf, size_t len)
{
	int ret = 0;

	ret = drm_dp_dpcd_write(mtk_edp_test->aux, dpcd_addr, buf, len);
	if (ret < 0)
		pr_info("%s %s 0x%x failed %d\n", EDP_PHY_AUTOTEST_DEBUG, __func__, dpcd_addr, ret);
}

static void mtk_edp_aux_read_access(struct dp_cts_auto_req *mtk_edp_test, u32 dpcd_addr,
					u8 *buf, u32 len)
{
	int ret = 0;

	ret = drm_dp_dpcd_read(mtk_edp_test->aux, dpcd_addr, buf, len);
	if (ret < 0)
		pr_info("%s %s 0x%x failed %d\n", EDP_PHY_AUTOTEST_DEBUG, __func__, dpcd_addr, ret);
}

static void mtk_edp_fec_enable(struct dp_cts_auto_req *mtk_edp_test, u8 enable)
{
	if (enable)
		mtk_edp_phy_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_4P_3540, BIT(0), BIT(0));
	else
		mtk_edp_phy_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_4P_3540, 0, BIT(0));
}

static bool mtk_edp_plug_state(struct dp_cts_auto_req *mtk_edp_test)
{
	return !!(mtk_edp_autotest_read(mtk_edp_test, REG_364C_AUX_TX_P0) &
		  HPD_STATUS_DP_AUX_TX_P0_MASK);
}

static void mtk_edp_training_set_scramble(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_3404,
			   enable ? DP_SCR_EN_DP_TRANS_P0_MASK : 0,
			   DP_SCR_EN_DP_TRANS_P0_MASK);
}

static void mtk_edp_phyd_reset_swing_pre(struct dp_cts_auto_req *mtk_edp_test)
{
	regmap_update_bits(mtk_edp_test->phyd_regs, PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(mtk_edp_test->phyd_regs, PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(mtk_edp_test->phyd_regs, PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
	regmap_update_bits(mtk_edp_test->phyd_regs, PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
			 EDP_TX_LN_VOLT_SWING_VAL_FLDMASK | EDP_TX_LN_PRE_EMPH_VAL_FLDMASK, 0x0);
}

static void mtk_edp_phy_reset(struct dp_cts_auto_req *mtk_edp_test)
{
	unsigned int val = 0x0;

	regmap_update_bits(mtk_edp_test->phyd_regs, MTK_DP_PHY_DIG_SW_RST,
			   0, DP_GLB_SW_RST_PHYD_MASK);
	usleep_range(50, 200);
	regmap_update_bits(mtk_edp_test->phyd_regs, MTK_DP_PHY_DIG_SW_RST,
			   DP_GLB_SW_RST_PHYD, DP_GLB_SW_RST_PHYD_MASK);

	regmap_read(mtk_edp_test->phyd_regs,DP_PHY_DIG_TX_CTL_0, &val);
	val = val & TX_LN_EN_FLDMASK;

	while (val > 0) {
		val >>= 1;
		regmap_update_bits(mtk_edp_test->phyd_regs, DP_PHY_DIG_TX_CTL_0,
			val, TX_LN_EN_FLDMASK);
	}

	mtk_edp_phyd_reset_swing_pre(mtk_edp_test);
}

static void mtk_edp_reset_swing_pre_emphasis(struct dp_cts_auto_req *mtk_edp_test)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TOP_SWING_EMP,
			   0,
			   DP_TX0_VOLT_SWING_MASK |
			   DP_TX1_VOLT_SWING_MASK |
			   DP_TX2_VOLT_SWING_MASK |
			   DP_TX3_VOLT_SWING_MASK |
			   DP_TX0_PRE_EMPH_MASK |
			   DP_TX1_PRE_EMPH_MASK |
			   DP_TX2_PRE_EMPH_MASK |
			   DP_TX3_PRE_EMPH_MASK);
}

static void mtk_edp_train_change_mode(struct dp_cts_auto_req *mtk_edp_test)
{
	mtk_edp_phy_reset(mtk_edp_test);
	mtk_edp_reset_swing_pre_emphasis(mtk_edp_test);
}

static int mtk_phy_configure(struct dp_cts_auto_req *mtk_edp_test,
		u32 link_rate, int lane_count, bool set_rate, bool set_lanes,
		bool set_voltages, unsigned int *swing, unsigned int *pre_emphasis)
{
	u32 val = 0x0;

	if (set_rate) {
		switch (link_rate) {
		case 1620:
			val = BIT_RATE_RBR;
			break;
		case 2700:
			val = BIT_RATE_HBR;
			break;
		case 5400:
			val = BIT_RATE_HBR2;
			break;
		case 8100:
			val = BIT_RATE_HBR3;
			break;
		default:
			pr_info("%s Implementation error, unknown linkrate 0x%x\n",
					EDP_PHY_AUTOTEST_DEBUG, link_rate);
			return -EINVAL;
		}
		mtk_edp_phy_autotest_write(mtk_edp_test, MTK_DP_PHY_DIG_BIT_RATE, val);
	}

	if (set_lanes) {
		for (val = 0; val < 4; val++) {
			mtk_edp_phy_autotest_update_bits(mtk_edp_test, DP_PHY_DIG_TX_CTL_0,
				((1 << (val + 1)) - 1), TX_LN_EN_FLDMASK);
		}
	}

	/* set swing and pre */
	if (set_voltages) {
		if (lane_count >= DPTX_LANE_COUNT1) {
			mtk_edp_phy_autotest_update_bits(mtk_edp_test,
				PHYD_DIG_LAN0_OFFSET + DRIVING_FORCE,
				EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
				EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
				swing[DPTX_LANE0] << 1 |
				pre_emphasis[DPTX_LANE0] << 3);

			if (lane_count >= DPTX_LANE_COUNT2) {
				mtk_edp_phy_autotest_update_bits(mtk_edp_test,
					PHYD_DIG_LAN1_OFFSET + DRIVING_FORCE,
					EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
					EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
					swing[DPTX_LANE1] << 1 |
					pre_emphasis[DPTX_LANE1] << 3);

				if (lane_count == DPTX_LANE_COUNT4) {
					mtk_edp_phy_autotest_update_bits(mtk_edp_test,
						PHYD_DIG_LAN2_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						swing[DPTX_LANE2] << 1 |
						pre_emphasis[DPTX_LANE2] << 3);

					mtk_edp_phy_autotest_update_bits(mtk_edp_test,
						PHYD_DIG_LAN3_OFFSET + DRIVING_FORCE,
						EDP_TX_LN_VOLT_SWING_VAL_FLDMASK |
						EDP_TX_LN_PRE_EMPH_VAL_FLDMASK,
						swing[DPTX_LANE3] << 1 |
						pre_emphasis[DPTX_LANE3] << 3);
				}
			}
		}
	}

	mtk_edp_phy_autotest_update_bits(mtk_edp_test, MTK_DP_PHY_DIG_PLL_CTL_1,
			   TPLL_SSC_EN, mtk_edp_test->training_info.sink_ssc_en ? 0 : TPLL_SSC_EN);

	return 0;
}

static int mtk_edp_phy_configure(struct dp_cts_auto_req *mtk_edp_test,u32 link_rate,
		int lane_count, bool set_swing_pre, unsigned int *swing, unsigned int *pre_emphasis)
{
	int ret = 0;

	ret = mtk_phy_configure(mtk_edp_test, link_rate, lane_count, TRUE, TRUE,
						set_swing_pre, swing, pre_emphasis);
	if (ret)
		return ret;

	/* Turn on phy power after phy configure */
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3FF8_DP_ENC_4P_3,
			   PHY_STATE_W_1_DP_ENC_4P_3, PHY_STATE_W_1_DP_ENC_4P_3_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL_LANE, DP_PWR_STATE_MASK);

	return 0;
}

static void mtk_edp_phyd_wait_aux_ldo_ready(struct dp_cts_auto_req *mtk_edp_test, unsigned long wait_us)
{
	int ret = 0;
	u32 val = 0x0;
	u32 mask = RGS_BG_CORE_EN_READY | RGS_AUX_LDO_EN_READY;

	ret = regmap_read_poll_timeout(mtk_edp_test->phyd_regs, DP_PHY_DIG_GLB_STATUS_0,
				val, !!(val & mask), wait_us/100, wait_us);

	if (ret)
		pr_info("%s %s AUX not ready\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
}

static void mtk_edp_set_lanes(struct dp_cts_auto_req *mtk_edp_test, u8 lanes)
{
	/* Turn off phy power before phy configure */
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3F44_DP_ENC_4P_3,
			   PHY_PWR_STATE_OW_EN_DP_ENC_4P_3, PHY_PWR_STATE_OW_EN_DP_ENC_4P_3_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3F44_DP_ENC_4P_3,
			   BIAS_POWER_ON, PHY_PWR_STATE_OW_VALUE_DP_ENC_4P_3_MASK);

	mtk_edp_phyd_wait_aux_ldo_ready(mtk_edp_test, 100000);

	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3F44_DP_ENC_4P_3,
			   0, PHY_PWR_STATE_OW_EN_DP_ENC_4P_3_MASK);

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_35F0,
			   lanes == 0 ? 0 : DP_TRANS_DUMMY_RW_0,
			   DP_TRANS_DUMMY_RW_0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3000,
			   lanes, LANE_NUM_DP_ENC0_P0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_34A4,
			   lanes << 2, LANE_NUM_DP_TRANS_P0_MASK);
}

static int mtk_edp_train_setting(struct dp_cts_auto_req *mtk_edp_test, u8 target_link_rate,
				u8 target_lane_count)
{
	u8 dpcd_tmp = 0x0;
	int ret;

	pr_info("%s %s+\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	dpcd_tmp = target_link_rate;
	mtk_edp_aux_write_access(mtk_edp_test, DP_LINK_BW_SET, &dpcd_tmp, sizeof(dpcd_tmp));
	dpcd_tmp = target_lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN;
	mtk_edp_aux_write_access(mtk_edp_test, DP_LANE_COUNT_SET, &dpcd_tmp, sizeof(dpcd_tmp));

	if (mtk_edp_test->training_info.sink_ssc_en)
		drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_DOWNSPREAD_CTRL, DP_SPREAD_AMP_0_5);

	mtk_edp_set_lanes(mtk_edp_test, target_lane_count / 2);
	ret = mtk_edp_phy_configure(mtk_edp_test, target_link_rate,
				target_lane_count, FALSE, NULL, NULL);
	if (ret)
		return ret;

	pr_info("%s Link train target_link_rate = 0x%x, target_lane_count = 0x%x\n",
		EDP_PHY_AUTOTEST_DEBUG, target_link_rate, target_lane_count);

	pr_info("%s %s-\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	return 0;
}

static void mtk_edp_set_idle_pattern(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	u32 val = POST_MISC_DATA_LANE0_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE1_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE2_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE3_OV_DP_TRANS_4P_MASK;

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_4P_3580,
			   enable ? val : 0, val);
}

static void mtk_edp_train_set_pattern(struct dp_cts_auto_req *mtk_edp_test, int pattern)
{
	/* TPS1 */
	if (pattern == 1)
		mtk_edp_set_idle_pattern(mtk_edp_test, false);

	mtk_edp_autotest_update_bits(mtk_edp_test,
			   MTK_DP_TRANS_P0_3400,
			   pattern ? BIT(pattern - 1) << 12 : 0,
			   PATTERN1_EN_DP_TRANS_P0_MASK |
			   PATTERN2_EN_DP_TRANS_P0_MASK |
			   PATTERN3_EN_DP_TRANS_P0_MASK |
			   PATTERN4_EN_DP_TRANS_P0_MASK);
}

static void mtk_edp_pattern(struct dp_cts_auto_req *mtk_edp_test, bool is_tps1)
{
	int pattern;
	unsigned int aux_offset;

	if (is_tps1) {
		pattern = 1;
		aux_offset = DP_LINK_SCRAMBLING_DISABLE | DP_TRAINING_PATTERN_1;
	} else {
		aux_offset =mtk_edp_test->training_info.channel_eq_pattern;

		switch (mtk_edp_test->training_info.channel_eq_pattern) {
		case DP_TRAINING_PATTERN_4:
			pattern = 4;
			break;
		case DP_TRAINING_PATTERN_3:
			pattern = 3;
			aux_offset |= DP_LINK_SCRAMBLING_DISABLE;
			break;
		case DP_TRAINING_PATTERN_2:
		default:
			pattern = 2;
			aux_offset |= DP_LINK_SCRAMBLING_DISABLE;
			break;
		}
	}

	mtk_edp_train_set_pattern(mtk_edp_test, pattern);
	drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_TRAINING_PATTERN_SET, aux_offset);
}

static void mtk_edp_set_swing_pre_emphasis(struct dp_cts_auto_req *mtk_edp_test, int lane_count,
					  unsigned int *swing_val, unsigned int *preemphasis)
{

	int lane = 0;

	for (lane = 0; lane < lane_count; lane++) {

		pr_info("%s Link training lane%d: swing_val = 0x%x, pre-emphasis = 0x%x\n",
			EDP_PHY_AUTOTEST_DEBUG, lane, swing_val[lane], preemphasis[lane]);
	}

	mtk_edp_phy_configure(mtk_edp_test, 0, lane_count, TRUE, swing_val, preemphasis);
}

static void mtk_edp_train_update_swing_pre(struct dp_cts_auto_req *mtk_edp_test, int lanes,
					  u8 dpcd_adjust_req[2])
{
	int lane;
	unsigned int swing[4]={};
	unsigned int preemphasis[4]={};

	for (lane = 0; lane < lanes; ++lane) {
		u8 val;
		int index = lane / 2;
		int shift = lane % 2 ? DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT : 0;

		swing[lane] = (dpcd_adjust_req[index] >> shift) &
			DP_ADJUST_VOLTAGE_SWING_LANE0_MASK;
		preemphasis[lane] = ((dpcd_adjust_req[index] >> shift) &
			       DP_ADJUST_PRE_EMPHASIS_LANE0_MASK) >>
			      DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
		val = swing[lane] << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      preemphasis[lane] << DP_TRAIN_PRE_EMPHASIS_SHIFT;

		if (swing[lane] == DP_TRAIN_VOLTAGE_SWING_LEVEL_3)
			val |= DP_TRAIN_MAX_SWING_REACHED;
		if (preemphasis[lane] == 3)
			val |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_TRAINING_LANE0_SET + lane,
				   val);
	}

	mtk_edp_set_swing_pre_emphasis(mtk_edp_test, lanes, swing, preemphasis);
}

static int mtk_edp_train_cr(struct dp_cts_auto_req *mtk_edp_test, u8 target_lane_count)
{
	u8 lane_adjust[2] = {};
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	u8 prev_lane_adjust = 0xff;
	int train_retries = 0;
	int voltage_retries = 0;
	int ret = 0;

	if (!target_lane_count)
		return -ENODEV;

	mtk_edp_pattern(mtk_edp_test, true);

	/* In DP spec 1.4, the retry count of CR is defined as 10. */
	do {
		train_retries++;
		if (!mtk_edp_plug_state(mtk_edp_test)) {
			mtk_edp_train_set_pattern(mtk_edp_test, 0);
			return -ENODEV;
		}

		ret = drm_dp_dpcd_read(mtk_edp_test->aux, DP_ADJUST_REQUEST_LANE0_1,
				 lane_adjust, sizeof(lane_adjust));
		if (ret < 0) {
			pr_info("%s %s failed to read adjust request %d\n",
					EDP_PHY_AUTOTEST_DEBUG, __func__, ret);
			return ret;
		}
		mtk_edp_train_update_swing_pre(mtk_edp_test, target_lane_count,
					      lane_adjust);

		drm_dp_link_train_clock_recovery_delay(mtk_edp_test->aux, mtk_edp_test->rx_cap);

		/* check link status from sink device */
		ret = drm_dp_dpcd_read_link_status(mtk_edp_test->aux, link_status);
		if (ret < 0) {
			pr_info("%s %s failed to link status %d\n",
					EDP_PHY_AUTOTEST_DEBUG, __func__, ret);
			return ret;
		}

		if (drm_dp_clock_recovery_ok(link_status,
					     target_lane_count)) {
			pr_info("%s CR training pass\n", EDP_PHY_AUTOTEST_DEBUG);
			return 0;
		}

		/*
		 * In DP spec 1.4, if current voltage level is the same
		 * with previous voltage level, we need to retry 5 times.
		 */
		if (prev_lane_adjust == link_status[4]) {
			voltage_retries++;
			/*
			 * Condition of CR fail:
			 * 1. Failed to pass CR using the same voltage
			 *    level over five times.
			 * 2. Failed to pass CR when the current voltage
			 *    level is the same with previous voltage
			 *    level and reach max voltage level (3).
			 */
			if (voltage_retries > MTK_DP_TRAIN_VOLTAGE_LEVEL_RETRY ||
			    (prev_lane_adjust & DP_ADJUST_VOLTAGE_SWING_LANE0_MASK) == 3) {
				pr_info("%s Link train CR fail\n", EDP_PHY_AUTOTEST_DEBUG);
				break;
			}
		} else {
			/*
			 * If the voltage level is changed, we need to
			 * re-calculate this retry count.
			 */
			voltage_retries = 0;
		}
		prev_lane_adjust = link_status[4];
		pr_info("%s CR training retries: %d\n", EDP_PHY_AUTOTEST_DEBUG, voltage_retries);
	} while (train_retries < MTK_DP_TRAIN_DOWNSCALE_RETRY);

	/* Failed to train CR, and disable pattern. */
	drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	mtk_edp_train_set_pattern(mtk_edp_test, 0);

	return -ETIMEDOUT;
}

static int mtk_edp_train_eq(struct dp_cts_auto_req *mtk_edp_test, u8 target_lane_count)
{
	u8 lane_adjust[2] = {};
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	int train_retries = 0;
	int ret = 0;

	if (!target_lane_count)
		return -ENODEV;

	mtk_edp_pattern(mtk_edp_test, false);

	do {
		train_retries++;
		if (!mtk_edp_plug_state(mtk_edp_test)) {
			mtk_edp_train_set_pattern(mtk_edp_test, 0);
			return -ENODEV;
		}

		ret = drm_dp_dpcd_read(mtk_edp_test->aux, DP_ADJUST_REQUEST_LANE0_1,
				 lane_adjust, sizeof(lane_adjust));
		if (ret < 0) {
			pr_info("%s %s failed to read adjust request %d\n",
					EDP_PHY_AUTOTEST_DEBUG, __func__, ret);
			return ret;
		}
		mtk_edp_train_update_swing_pre(mtk_edp_test, target_lane_count,
					      lane_adjust);

		drm_dp_link_train_channel_eq_delay(mtk_edp_test->aux, mtk_edp_test->rx_cap);

		/* check link status from sink device */
		ret = drm_dp_dpcd_read_link_status(mtk_edp_test->aux, link_status);
		if (ret < 0) {
			pr_info("%s %s failed to link status %d\n",
					EDP_PHY_AUTOTEST_DEBUG, __func__, ret);
			return ret;
		}

		if (drm_dp_channel_eq_ok(link_status, target_lane_count)) {
			pr_info("%s EQ training pass\n", EDP_PHY_AUTOTEST_DEBUG);

			/* Training done, and disable pattern. */
			drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_TRAINING_PATTERN_SET,
					   DP_TRAINING_PATTERN_DISABLE);
			mtk_edp_train_set_pattern(mtk_edp_test, 0);
			return 0;
		}
		pr_info("%s EQ training retries: %d\n", EDP_PHY_AUTOTEST_DEBUG, train_retries);
	} while (train_retries < MTK_DP_TRAIN_DOWNSCALE_RETRY);

	/* Failed to train EQ, and disable pattern. */
	drm_dp_dpcd_writeb(mtk_edp_test->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	mtk_edp_train_set_pattern(mtk_edp_test, 0);

	return -ETIMEDOUT;
}

static void mtk_edp_set_enhanced_frame_mode(struct dp_cts_auto_req *mtk_edp_test)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3000,
			   ENHANCED_FRAME_EN_DP_ENC0_P0,
			   ENHANCED_FRAME_EN_DP_ENC0_P0);
}

static int mtk_edp_set_training_start_for_cts(struct dp_cts_auto_req *mtk_edp_test)
{
	int ret;
	u8 lane_count, link_rate, train_limit, max_link_rate;

	link_rate = mtk_edp_test->training_info.link_rate;
	lane_count = mtk_edp_test->training_info.link_lane_count;
	max_link_rate = link_rate;
	pr_info("%s %s+\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	pr_info("%s RX support link rate= 0x%x, lane_count= 0x%x\n", EDP_PHY_AUTOTEST_DEBUG,
			link_rate, lane_count);

	/*
	 * TPS are generated by the hardware pattern generator. From the
	 * hardware setting we need to disable this scramble setting before
	 * use the TPS pattern generator.
	 */
	mtk_edp_training_set_scramble(mtk_edp_test, false);

	for (train_limit = 6; train_limit > 0; train_limit--) {
		mtk_edp_train_change_mode(mtk_edp_test);

		ret = mtk_edp_train_setting(mtk_edp_test, link_rate, lane_count);
		if (ret)
			return ret;

		ret = mtk_edp_train_cr(mtk_edp_test, lane_count);
		if (ret == -ENODEV) {
			return ret;
		} else if (ret) {
			/* reduce link rate */
			switch (link_rate) {
			case DP_LINK_BW_1_62:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;
				if (lane_count == 0)
					return -EIO;
				break;
			case DP_LINK_BW_2_7:
				link_rate = DP_LINK_BW_1_62;
				break;
			case DP_LINK_BW_5_4:
				link_rate = DP_LINK_BW_2_7;
				break;
			case DP_LINK_BW_8_1:
				link_rate = DP_LINK_BW_5_4;
				break;
			default:
				return -EINVAL;
			}
			continue;
		}

		ret = mtk_edp_train_eq(mtk_edp_test, lane_count);
		if (ret == -ENODEV) {
			return ret;
		} else if (ret) {
			/* reduce lane count */
			if (lane_count == 0)
				return -EIO;
			lane_count /= 2;
			continue;
		}
		/* if we can run to this, training is done. */
		break;
	}

	if (train_limit == 0)
		return -ETIMEDOUT;

	mtk_edp_test->training_info.link_rate = link_rate;
	mtk_edp_test->training_info.link_lane_count = lane_count;

	/*
	 * After training done, we need to output normal stream instead of TPS,
	 * so we need to enable scramble.
	 */
	mtk_edp_training_set_scramble(mtk_edp_test, true);
	mtk_edp_set_enhanced_frame_mode(mtk_edp_test);

	pr_info("%s %s-\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	return 0;
}

static void mtk_edp_test_link_training(struct dp_cts_auto_req *mtk_edp_test)
{
	u8 test_link_rate;
	u8 test_lane_count;
	u8 dpcd_tmp = 0x0;
	int ret = 0;

	pr_info("%s %s+\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	pr_info("%s TEST_LINK_TRAINING Start\n", EDP_PHY_AUTOTEST_DEBUG);
	dpcd_tmp = 0x01;
	mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE, &dpcd_tmp, 0x1);
	mtk_edp_aux_read_access(mtk_edp_test, DP_TEST_LINK_RATE, &test_link_rate, 0x1);
	mtk_edp_aux_read_access(mtk_edp_test, DP_TEST_LANE_COUNT, &test_lane_count, 0x1);
	ret = drm_dp_read_dpcd_caps(mtk_edp_test->aux, mtk_edp_test->rx_cap);
	if (ret < 0)
		return;

	mtk_edp_test->training_info.sink_ssc_en = drm_dp_max_downspread(mtk_edp_test->rx_cap);
	if (drm_dp_tps4_supported(mtk_edp_test->rx_cap)) {
		mtk_edp_test->training_info.tps4_support = TRUE;
		mtk_edp_test->training_info.channel_eq_pattern = DP_TRAINING_PATTERN_4;
	} else if (drm_dp_tps3_supported(mtk_edp_test->rx_cap)) {
		mtk_edp_test->training_info.tps3_support = TRUE;
		mtk_edp_test->training_info.channel_eq_pattern = DP_TRAINING_PATTERN_3;
	} else
		mtk_edp_test->training_info.channel_eq_pattern = DP_TRAINING_PATTERN_2;

	if (test_link_rate && test_lane_count) {
		mtk_edp_test->training_info.link_rate = test_link_rate;
		mtk_edp_test->training_info.link_lane_count = test_lane_count;

		dpcd_tmp = 0x0;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TRAINING_PATTERN_SET, &dpcd_tmp, 0x1);
		dpcd_tmp = 0x2;
		mtk_edp_aux_write_access(mtk_edp_test, DP_SET_POWER, &dpcd_tmp, 0x1);
		mtk_edp_fec_enable(mtk_edp_test, FALSE);
		ret = mtk_edp_set_training_start_for_cts(mtk_edp_test);
		if (ret)
			pr_info("%s link training test failed %d\n", EDP_PHY_AUTOTEST_DEBUG, ret);

		mtk_edp_fec_enable(mtk_edp_test, mtk_edp_test->training_info.sink_ssc_en);
	}

	pr_info("%s %s-\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
}

static void mtk_edp_set_misc(struct dp_cts_auto_req *mtk_edp_test, u8 ucMISC[2])
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
		ucMISC[0], 0xFE);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
		ucMISC[1] << 8, 0xFF << 8);
}

static void mtk_edp_set_color_format(struct dp_cts_auto_req *mtk_edp_test, u8 color_format)
{
	u32 val = 0;
	u32 misc0 = 0;

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
			   BIT(3), BIT(3));

	switch (color_format) {
	case TEST_COLOR_FORMAT_RGB:
		misc0 = 0x0;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	case TEST_COLOR_FORMAT_YUV444:
		misc0 = 0x2;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	case TEST_COLOR_FORMAT_YUV422:
		misc0 = 0x1;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR422;
		break;
	default:
		pr_info("%s Not supported color format: %d\n", EDP_PHY_AUTOTEST_DEBUG, color_format);
		return;
	}

	/* update MISC0 for color format */
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
			   misc0 << DP_TEST_COLOR_FORMAT_SHIFT,
			   DP_TEST_COLOR_FORMAT_MASK);

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_303C,
			   val, PIXEL_ENCODE_FORMAT_DP_ENC0_P0_MASK);
}

static void mtk_edp_set_color_depth(struct dp_cts_auto_req *mtk_edp_test, u8 coloer_depth)
{
	u32 val = 0;
	u32 misc0 = 0;

	switch (coloer_depth) {
	case TEST_BIT_DEPTH_6:
		misc0 = DP_MSA_MISC_6_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_6BIT;
		break;
	case TEST_BIT_DEPTH_8:
		misc0 = DP_MSA_MISC_8_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;

		/* set MISC0 BT709 */
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
				MTK_DP_MISC0_BT709, MTK_DP_MISC0_BT_MASK);
		break;
	case TEST_BIT_DEPTH_10:
		misc0 = DP_MSA_MISC_10_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_10BIT;

		/* set MISC0 BT601 */
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
				MTK_DP_MISC0_BT601, MTK_DP_MISC0_BT_MASK);
		break;
	case TEST_BIT_DEPTH_12:
		misc0 = DP_MSA_MISC_12_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_12BIT;
		break;
	case TEST_BIT_DEPTH_16:
		misc0 = DP_MSA_MISC_16_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_16BIT;
		break;
	default:
		pr_info("%s Not supported color depth: %d\n", EDP_PHY_AUTOTEST_DEBUG, coloer_depth);
		break;
	}

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3034,
				misc0, DP_TEST_BIT_DEPTH_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_303C,
				val, VIDEO_COLOR_DEPTH_DP_ENC0_P0_MASK);
}

static void mtk_edp_set_msa(struct dp_cts_auto_req *mtk_edp_test)
{
	struct dp_cts_auto_dptx_timing *DPTX_TBL = &mtk_edp_test->dp_cts_outbl;

	/* horizontal */
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3010, DPTX_TBL->Htt);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3018,
		DPTX_TBL->Hsw + DPTX_TBL->Hbp);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3028,
		DPTX_TBL->Hsw << HSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSW_SW_DP_ENCODER0_P0_FLDMASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3028,
		DPTX_TBL->bHsp << HSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		HSP_SW_DP_ENCODER0_P0_FLDMASK);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3020, DPTX_TBL->Hde);


	/* vertical */
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3014, DPTX_TBL->Vtt);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_301C,
		DPTX_TBL->Vsw + DPTX_TBL->Vbp);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_302C,
		DPTX_TBL->Vsw << VSW_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSW_SW_DP_ENCODER0_P0_FLDMASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_302C,
		DPTX_TBL->bVsp << VSP_SW_DP_ENCODER0_P0_FLDMASK_POS,
		VSP_SW_DP_ENCODER0_P0_FLDMASK);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3024, DPTX_TBL->Vde);

	/* horizontal */
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3064, DPTX_TBL->Hde);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3154, (DPTX_TBL->Htt));
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3158, (DPTX_TBL->Hfp));
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_315C, (DPTX_TBL->Hsw));
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3160,
		DPTX_TBL->Hbp + DPTX_TBL->Hsw);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3164, (DPTX_TBL->Hde));

	/* vertical */
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3168, DPTX_TBL->Vtt);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_316C, DPTX_TBL->Vfp);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3170, DPTX_TBL->Vsw);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3174,
		DPTX_TBL->Vbp + DPTX_TBL->Vsw);
	mtk_edp_autotest_write(mtk_edp_test, MTK_DP_ENC0_P0_3178, DPTX_TBL->Vde);

	pr_info("MSA:Htt=%d Vtt=%d Hact=%d Vact=%d, fps=%d\n",
			DPTX_TBL->Htt, DPTX_TBL->Vtt,
			DPTX_TBL->Hde, DPTX_TBL->Vde, DPTX_TBL->FrameRate);
}

static void mtk_edp_enable_bypass_msa(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	u32 mask = HTOTAL_SEL_DP_ENC0_P0 | VTOTAL_SEL_DP_ENC0_P0 |
		   HSTART_SEL_DP_ENC0_P0 | VSTART_SEL_DP_ENC0_P0 |
		   HWIDTH_SEL_DP_ENC0_P0 | VHEIGHT_SEL_DP_ENC0_P0 |
		   HSP_SEL_DP_ENC0_P0 | HSW_SEL_DP_ENC0_P0 |
		   VSP_SEL_DP_ENC0_P0 | VSW_SEL_DP_ENC0_P0;

	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_3030, enable ? 0 : mask, mask);
}

static void mtk_edp_edptx_calculate_mn(struct dp_cts_auto_req *mtk_edp_test)
{
	int ubTargetFrameRate = 60;
	int ulTargetPixelclk = 148500000; /* default set FHD */

	if (mtk_edp_test->dp_cts_outbl.FrameRate > 0) {
		ubTargetFrameRate = mtk_edp_test->dp_cts_outbl.FrameRate;
		ulTargetPixelclk = (int)mtk_edp_test->dp_cts_outbl.Htt *
			(int)mtk_edp_test->dp_cts_outbl.Vtt * ubTargetFrameRate;
	} else if (mtk_edp_test->dp_cts_outbl.PixRateKhz > 0) {
		ulTargetPixelclk = mtk_edp_test->dp_cts_outbl.PixRateKhz * 1000;
	} else {
		ulTargetPixelclk = (int)mtk_edp_test->dp_cts_outbl.Htt *
			(int)mtk_edp_test->dp_cts_outbl.Vtt * ubTargetFrameRate;
	}

	if (ulTargetPixelclk > 0)
		mtk_edp_test->dp_cts_outbl.PixRateKhz = ulTargetPixelclk / 1000;
}

static void mtk_edp_edptx_pg_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3038,
			   enable ? VIDEO_SOURCE_SEL_DP_ENC0_P0_MASK : 0,
			   VIDEO_SOURCE_SEL_DP_ENC0_P0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_31B0,
			   PGEN_PATTERN_SEL_VAL << 4, PGEN_PATTERN_SEL_MASK);
}

static void mtk_edp_set_mvidx2(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_300C  + 1,
			BIT(4), BIT(6)|BIT(5)|BIT(4));
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_300C  + 1,
			0, BIT(6)|BIT(5)|BIT(4));
}

static u8 mtk_edp_get_color_bpp(struct dp_cts_auto_req *mtk_edp_test)
{
	u8 ColorBpp;
	u8 ubDPTXColorDepth = mtk_edp_test->test_bit_depth;
	u8 ubDPTXColorFormat = mtk_edp_test->test_color_fmt;

	switch (ubDPTXColorDepth) {
	case DP_CTS_COLOR_DEPTH_6BIT:
		if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 18;
		break;
	case DP_CTS_COLOR_DEPTH_8BIT:
		if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 24;
		break;
	case DP_CTS_COLOR_DEPTH_10BIT:
		if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_422)
			ColorBpp = 20;
		else if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_420)
			ColorBpp = 15;
		else
			ColorBpp = 30;
		break;
	case DP_CTS_COLOR_DEPTH_12BIT:
		if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_422)
			ColorBpp = 24;
		else if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_420)
			ColorBpp = 18;
		else
			ColorBpp = 36;
		break;
	case DP_CTS_COLOR_DEPTH_16BIT:
		if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_422)
			ColorBpp = 32;
		else if (ubDPTXColorFormat == DP_CTS_COLOR_FORMAT_YUV_420)
			ColorBpp = 24;
		else
			ColorBpp = 48;
		break;
	default:
		ColorBpp = 24;
		pr_info("%s get wrong bpp = %d\n", EDP_PHY_AUTOTEST_DEBUG, ColorBpp);
		break;
	}

	return ColorBpp;
}

static void mtk_edp_set_sram_read_start(struct dp_cts_auto_req *mtk_edp_test, u16 uwValue)
{
	/* [5:0]video sram start address=>modify in 480P case only, default=0x1F */
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_303C,
						uwValue, SRAM_START_READ_THRD_DP_ENC0_P0_MASK);
}

static void mtk_edp_set_tu_set_encoder(struct dp_cts_auto_req *mtk_edp_test)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_4P_303C,
			   VIDEO_MN_GEN_EN_DP_ENC0_P0,
			   VIDEO_MN_GEN_EN_DP_ENC0_P0);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3040,
			   SDP_DOWN_CNT_DP_ENC0_P0_VAL,
			   SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC1_P0_3364,
			   SDP_DOWN_CNT_IN_HBLANK_DP_ENC1_P0_VAL,
			   SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC1_P0_3300,
			   VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_VAL << 8,
			   VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_MASK);
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC1_P0_3364,
			   FIFO_READ_START_POINT_DP_ENC1_P0_VAL << 12,
			   FIFO_READ_START_POINT_DP_ENC1_P0_MASK);

	mtk_edp_autotest_write(mtk_edp_test, REG_3368_DP_ENCODER1_P0, 0x1111);
}

static void mtk_edp_set_sdp_down_cntinit_in_hblanking(struct dp_cts_auto_req *mtk_edp_test,
	u16 uwValue)
{
	 /* [11 : 0]mtk_dp, REG_sdp_down_cnt_init_in_hblank */
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC1_P0_3364, uwValue, 0x0FFF);

}

static void mtk_edp_sdp_set_down_cnt_init_in_hblank(struct dp_cts_auto_req *mtk_edp_test)
{
	int PixClkMhz;
	u8 ucDCOffset;

	PixClkMhz = (mtk_edp_test->test_color_fmt == DP_CTS_COLOR_FORMAT_YUV_420) ?
		mtk_edp_test->dp_cts_outbl.PixRateKhz / 2000 :
		mtk_edp_test->dp_cts_outbl.PixRateKhz / 1000;

	switch (mtk_edp_test->ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		mtk_edp_set_sdp_down_cntinit_in_hblanking(mtk_edp_test, 0x0020);
		break;
	case DP_LANECOUNT_2:
		ucDCOffset = (mtk_edp_test->dp_cts_outbl.Vtt <= 525) ? 0x14 : 0x00;
		mtk_edp_set_sdp_down_cntinit_in_hblanking(mtk_edp_test,
			0x0018 + ucDCOffset);
		break;
	case DP_LANECOUNT_4:
		ucDCOffset = (mtk_edp_test->dp_cts_outbl.Vtt <= 525) ? 0x08 : 0x00;
		if (PixClkMhz > (mtk_edp_test->ubLinkRate * 27)) {
			mtk_edp_set_sdp_down_cntinit_in_hblanking(mtk_edp_test, 0x0008);
			pr_info("%s Pixclk > LinkRateChange\n", EDP_PHY_AUTOTEST_DEBUG);
		} else {
			mtk_edp_set_sdp_down_cntinit_in_hblanking(mtk_edp_test,
				0x0010 + ucDCOffset);
		}
		break;
	}
}

static void mtk_edp_set_sdp_down_cntinit(struct dp_cts_auto_req *mtk_edp_test,
	u16 Sram_Read_Start)
{
	u16 SDP_Down_Cnt_Init = 0x0000;
	u8 ucDCOffset;

	if (mtk_edp_test->dp_cts_outbl.PixRateKhz > 0)
		SDP_Down_Cnt_Init = (Sram_Read_Start *
			mtk_edp_test->ubLinkRate * 2700 * 8)
			/ (mtk_edp_test->dp_cts_outbl.PixRateKhz * 4);

	switch (mtk_edp_test->ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x1A) ?
			SDP_Down_Cnt_Init : 0x1A;  /* 26 */
		break;
	case DP_LANECOUNT_2:
		/* case for LowResolution && High Audio Sample Rate */
		ucDCOffset = (mtk_edp_test->dp_cts_outbl.Vtt <= 525) ?
			0x04 : 0x00;
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x10) ?
			SDP_Down_Cnt_Init : 0x10 + ucDCOffset; /* 20 or 16 */
		break;
	case DP_LANECOUNT_4:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x06) ?
			SDP_Down_Cnt_Init : 0x06; /* 6 */
		break;
	default:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x06) ?
			SDP_Down_Cnt_Init : 0x06;
		break;
	}

	pr_info("%s PixRateKhz = %d SDP_DC_Init = %x\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->dp_cts_outbl.PixRateKhz, SDP_Down_Cnt_Init);

	/* [11 : 0]mtk_dp, REG_sdp_down_cnt_init */
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3040,
						SDP_Down_Cnt_Init, 0x0FFF);
}

static void mtk_edp_set_tu(struct dp_cts_auto_req *mtk_edp_test)
{
	int TU_size = 0;
	int NValue = 0;
	int FValue = 0;
	int PixRateMhz = 0;
	u8 ColorBpp;
	u16 Sram_Read_Start = DPTX_TBC_BUF_ReadStartAdrThrd;

	ColorBpp = mtk_edp_get_color_bpp(mtk_edp_test);
	PixRateMhz = mtk_edp_test->dp_cts_outbl.PixRateKhz / 1000;
	TU_size = (640 * (PixRateMhz) * ColorBpp)/
			(mtk_edp_test->ubLinkRate * 27 *
				mtk_edp_test->ubLinkLaneCount * 8);

	NValue = TU_size / 10;
	FValue = TU_size - NValue * 10;

	pr_info("%s TU_size %d, FValue %d\n", EDP_PHY_AUTOTEST_DEBUG, TU_size, FValue);
	if (mtk_edp_test->ubLinkLaneCount > 0) {
		Sram_Read_Start = mtk_edp_test->dp_cts_outbl.Hde /
			(mtk_edp_test->ubLinkLaneCount* 4 * 2 * 2);
		Sram_Read_Start =
			(Sram_Read_Start < DPTX_TBC_BUF_ReadStartAdrThrd) ?
			Sram_Read_Start : DPTX_TBC_BUF_ReadStartAdrThrd;
		mtk_edp_set_sram_read_start(mtk_edp_test, Sram_Read_Start);
	}

	mtk_edp_set_tu_set_encoder(mtk_edp_test);
	mtk_edp_sdp_set_down_cnt_init_in_hblank(mtk_edp_test);
	mtk_edp_set_sdp_down_cntinit(mtk_edp_test, Sram_Read_Start);
}

static void mtk_edp_set_edptx_out(struct dp_cts_auto_req *mtk_edp_test)
{
	mtk_edp_enable_bypass_msa(mtk_edp_test, false);
	mtk_edp_edptx_calculate_mn(mtk_edp_test);

	switch (mtk_edp_test->input_src) {
	case DPTX_SRC_PG:
		mtk_edp_edptx_pg_enable(mtk_edp_test, true);
		mtk_edp_set_mvidx2(mtk_edp_test, false);
		pr_info("%s Set Pattern Gen output\n", EDP_PHY_AUTOTEST_DEBUG);
		break;

	case DPTX_SRC_DPINTF:
		mtk_edp_edptx_pg_enable(mtk_edp_test, false);
		pr_info("%s Set dpintf output\n", EDP_PHY_AUTOTEST_DEBUG);
		break;

	default:
		mtk_edp_edptx_pg_enable(mtk_edp_test, true);
		break;
	}

	mtk_edp_set_tu(mtk_edp_test);
}

void mtk_edp_video_mute(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	struct arm_smccc_res res;
	u32 x3 = (EDP_VIDEO_UNMUTE << 16) | enable;
	u32 val = VIDEO_MUTE_SEL_DP_ENC0_P0 |
			(enable ? VIDEO_MUTE_SW_DP_ENC0_P0 : 0);

	/* use secure mute and MTK_DP_ENC0_P0_3000 use default mute value */
	mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_ENC0_P0_3000,
			   val, VIDEO_MUTE_SEL_DP_ENC0_P0 | VIDEO_MUTE_SW_DP_ENC0_P0);

	arm_smccc_smc(MTK_SIP_DP_CONTROL,
		      EDP_VIDEO_UNMUTE, enable,
		      x3, 0xFEFD, 0, 0, 0, &res);

	pr_info("%s smc cmd: 0x%x, p1: %s, ret: 0x%lx-0x%lx\n",
		EDP_PHY_AUTOTEST_DEBUG, EDP_VIDEO_UNMUTE, enable ? "enable" : "disable", res.a0, res.a1);
}

static void mtk_edp_test_video_pattern(struct dp_cts_auto_req *mtk_edp_test)
{
	u8 i = 0x0;
	/* 220~22F */
	u8 dpcd_22x[0x10] = {0x0};
	/* 230~234 */
	u8 dpcd_23x[0x5] = {0x0};
	/* 271~27F */
	u8 dpcd_27x[0xa] = {0x0};
	u8 ucMISC[2] = {0x00};

	pr_info("%s %s+\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	memset(dpcd_22x, 0x0, sizeof(dpcd_22x));
	memset(dpcd_23x, 0x0, sizeof(dpcd_23x));
	memset(dpcd_27x, 0x0, sizeof(dpcd_27x));
	mtk_edp_aux_read_access(mtk_edp_test, DPCD_00220, dpcd_22x, 0x10);
	mtk_edp_aux_read_access(mtk_edp_test, DPCD_00230, dpcd_22x, 0x5);
	mtk_edp_aux_read_access(mtk_edp_test, DPCD_00271, dpcd_22x, 0xa);

	for (i = 0; i < 0xa; i++)
		pr_info("%s dpcd_27%d = 0x%x\n", EDP_PHY_AUTOTEST_DEBUG, i+1, dpcd_27x[i]);

	mtk_edp_test->input_src = DPTX_SRC_DPINTF;
	mtk_edp_test->test_pattern = dpcd_22x[0x1];
	pr_info("%s test video pattern type dpcd_221 = 0x%x\n",
			EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_pattern);

	mtk_edp_test->test_h_total = ((dpcd_22x[2]<<8) + (dpcd_22x[3]));
	mtk_edp_test->test_v_total = ((dpcd_22x[4]<<8) + (dpcd_22x[5]));
	mtk_edp_test->test_h_start = ((dpcd_22x[6]<<8) + (dpcd_22x[7]));
	mtk_edp_test->test_v_start = ((dpcd_22x[8]<<8) + (dpcd_22x[9]));
	mtk_edp_test->test_hsync_polarity = ((dpcd_22x[0xa] & 0x80)>>7);
	mtk_edp_test->test_hsync_width =
		(((dpcd_22x[0xa] & 0x7f)<<8) + (dpcd_22x[0xb]));
	mtk_edp_test->test_vsync_polarity = ((dpcd_22x[0xc] & 0x80)>>7);
	mtk_edp_test->test_vsync_width =
		(((dpcd_22x[0xc] & 0x7f)<<8) + (dpcd_22x[0xd]));
	mtk_edp_test->test_h_width = ((dpcd_22x[0xe]<<8) + (dpcd_22x[0xf]));
	mtk_edp_test->test_v_height = ((dpcd_23x[0]<<8) + (dpcd_23x[1]));
	mtk_edp_test->test_sync_clk = (dpcd_23x[2] & 0x1);
	mtk_edp_test->test_color_fmt = ((dpcd_23x[2] & 0x6)>>1);
	mtk_edp_test->test_dynamic_range = ((dpcd_23x[2] & 0x8)>>3);
	mtk_edp_test->test_YCbCr_coefficient = ((dpcd_23x[2] & 0x10)>>4);
	mtk_edp_test->test_bit_depth = ((dpcd_23x[2] & 0xe0)>>5);
	mtk_edp_test->test_refresh_denominator = (dpcd_23x[3] & 0x1);
	mtk_edp_test->test_interlaced = ((dpcd_23x[3] & 0x2)>>1);
	mtk_edp_test->test_refresh_rate_numerator = (dpcd_23x[4]);

	pr_info("%s %s test request:\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	pr_info("%s req.test_pattern = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_pattern);
	pr_info("%s req.test_h_total = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_h_total);
	pr_info("%s req.test_v_total = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_v_total);
	pr_info("%s req.test_h_start = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_h_start);
	pr_info("%s req.test_v_start = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_v_start);
	pr_info("%s req.test_hsync_polarity = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_hsync_polarity);
	pr_info("%s req.test_hsync_width = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_hsync_width);
	pr_info("%s req.test_vsync_polarity = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_vsync_polarity);
	pr_info("%s req.test_vsync_width =  %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_vsync_width);
	pr_info("%s req.test_h_width = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_h_width);
	pr_info("%s req.test_v_height = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_v_height);
	pr_info("%s req.test_sync_clk = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_sync_clk);
	pr_info("%s req.test_color_fmt = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_color_fmt);
	pr_info("%s req.test_dynamic_range = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_dynamic_range);
	pr_info("%s req.test_YCbCr_coefficient = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_YCbCr_coefficient);
	pr_info("%s req.test_bit_depth = %d\n", EDP_PHY_AUTOTEST_DEBUG, mtk_edp_test->test_bit_depth);
	pr_info("%s req.test_refresh_denominator = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_refresh_denominator);
	pr_info("%s req.test_interlaced = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_interlaced);
	pr_info("%s req.test_refresh_rate_numerator = %d\n", EDP_PHY_AUTOTEST_DEBUG,
		mtk_edp_test->test_refresh_rate_numerator);

	mtk_edp_test->dp_cts_outbl.Htt = mtk_edp_test->test_h_total;
	mtk_edp_test->dp_cts_outbl.Hde = mtk_edp_test->test_h_width;
	mtk_edp_test->dp_cts_outbl.Hfp = mtk_edp_test->test_h_total - mtk_edp_test->test_h_start -
							   mtk_edp_test->test_h_width;
	mtk_edp_test->dp_cts_outbl.Hsw = mtk_edp_test->test_hsync_width;
	mtk_edp_test->dp_cts_outbl.bHsp = mtk_edp_test->test_hsync_polarity;
	mtk_edp_test->dp_cts_outbl.Hbp = mtk_edp_test->test_h_start - mtk_edp_test->test_hsync_width;
	mtk_edp_test->dp_cts_outbl.Vtt = mtk_edp_test->test_v_total;
	mtk_edp_test->dp_cts_outbl.Vde = mtk_edp_test->test_v_height;
	mtk_edp_test->dp_cts_outbl.Vfp = mtk_edp_test->test_v_total - mtk_edp_test->test_v_start -
							   mtk_edp_test->test_v_height;
	mtk_edp_test->dp_cts_outbl.Vsw = mtk_edp_test->test_vsync_width;
	mtk_edp_test->dp_cts_outbl.bVsp = mtk_edp_test->test_vsync_polarity;
	mtk_edp_test->dp_cts_outbl.Vbp = mtk_edp_test->test_v_start - mtk_edp_test->test_vsync_width;
	mtk_edp_test->dp_cts_outbl.Hbk = mtk_edp_test->test_h_total - mtk_edp_test->test_h_width;
	mtk_edp_test->dp_cts_outbl.FrameRate = mtk_edp_test->test_refresh_rate_numerator;
	mtk_edp_test->dp_cts_outbl.PixRateKhz = 0;
	mtk_edp_test->dp_cts_outbl.Video_ip_mode = mtk_edp_test->test_interlaced;
	if (mtk_edp_test->test_interlaced & TEST_INTERLACED)
		pr_info("%s Warning: not support interlace\n", EDP_PHY_AUTOTEST_DEBUG);

	mtk_edp_test->dp_cts_outbl.Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;

	/* Clear MISC1&0 except [2:1]Colorfmt & [7:5]ColorDepth */
	mtk_edp_set_misc(mtk_edp_test, ucMISC);

	// mtk_edp_test->training_info.DPTX_MISC.dp_misc.spec_def1 = mtk_edp_test->test_dynamic_range;

	mtk_edp_set_color_format(mtk_edp_test, mtk_edp_test->test_color_fmt);
	mtk_edp_set_color_depth(mtk_edp_test, mtk_edp_test->test_bit_depth);
	mtk_edp_set_msa(mtk_edp_test);
	mtk_edp_set_edptx_out(mtk_edp_test);
	mtk_edp_video_mute(mtk_edp_test, false);

	pr_info("%s %s-\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
}

static struct edid *mtk_edp_handle_edid(struct dp_cts_auto_req *mtk_edp_test)
{
	/* use cached edid if we have one */
	if (mtk_edp_test->edid) {
		/* invalid edid */
		if (IS_ERR(mtk_edp_test->edid))
			return NULL;

		pr_info("%s duplicate edid from mtk_dp->edid!\n", EDP_PHY_AUTOTEST_DEBUG);
		return drm_edid_duplicate(mtk_edp_test->edid);
	}

	pr_info("%s Get edid from RX!\n", EDP_PHY_AUTOTEST_DEBUG);
	return drm_get_edid(mtk_edp_test->connector, &mtk_edp_test->aux->ddc);
}

static void mtk_edp_test_edid_read(struct dp_cts_auto_req *mtk_edp_test)
{
	u8 dpcd_tmp = 0x0;

	pr_info("%s TEST_EDID_READ Start\n", EDP_PHY_AUTOTEST_DEBUG);
	kfree(mtk_edp_test->edid);
	mtk_edp_test->edid = mtk_edp_handle_edid(mtk_edp_test);
	if (!mtk_edp_test->edid) {
		pr_info("%s EDID is NULL\n", EDP_PHY_AUTOTEST_DEBUG);
		return;
	}

	msleep(20);
	dpcd_tmp = mtk_edp_test->edid->checksum;
	mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_EDID_CHECKSUM,
						&dpcd_tmp, sizeof(dpcd_tmp));
	dpcd_tmp = BIT(0) | BIT(2);
	mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
								&dpcd_tmp, sizeof(dpcd_tmp));
}

static void mtk_edp_set_training_pattern(struct dp_cts_auto_req *mtk_edp_test, enum DPTX_LT_PATTERN val)
{
	if (val <= DPTX_TPS4) {
		pr_info("%s Set Train Pattern = 0x%x\n", EDP_PHY_AUTOTEST_DEBUG, val);
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_3400, val, GENMASK(7, 4));
	}
	mdelay(20);
}

static void mtk_edp_phy_set_lane_pwr(struct dp_cts_auto_req *mtk_edp_test, u8 lane_count)
{
	int i = 0;

	for (i = 0; i < lane_count; i++)
		mtk_edp_phy_autotest_update_bits(mtk_edp_test, DP_PHY_DIG_TX_CTL_0,
							(BIT(0)) << i, TX_LN_EN_FLDMASK);
}

static void mtk_edp_pattern_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3440_DP_TRANS_4P,
						0xF, REG_PGM_PAT_EN_DP_TRANS_4P_MASK);
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3440_DP_TRANS_4P,
						0x0, REG_PGM_PAT_EN_DP_TRANS_4P_MASK);
}

static void mtk_edp_prbs_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3444_DP_TRANS_4P,
						BIT(3), REG_PRBS_EN_DP_TRANS_4P_MASK);
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3444_DP_TRANS_4P,
						0, REG_PRBS_EN_DP_TRANS_4P_MASK);
}

static void mtk_edp_pattern_select(struct dp_cts_auto_req *mtk_edp_test, enum DPTX_PG_TYPE pattern)
{
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3440_DP_TRANS_4P,
							(pattern << 4), GENMASK(6, 4));
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3440_DP_TRANS_4P,
							(pattern << 8), GENMASK(10, 8));
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3440_DP_TRANS_4P,
							(pattern << 12), GENMASK(14, 12));
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3444_DP_TRANS_4P,
							pattern, GENMASK(2, 0));
}

static void mtk_edp_lane_pn_swap(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable) {
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				BIT(4), REG_POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				BIT(5), REG_POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				BIT(6), REG_POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				BIT(7), REG_POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_4P);
	} else {
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				0, REG_POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				0, REG_POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				0, REG_POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_4P);
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3428_DP_TRANS_4P,
				0, REG_POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_4P);
	}
}

static void mtk_edp_set_program_pattern(struct dp_cts_auto_req *mtk_edp_test, u8 val,
				      u8 *data)
{
	/* 16bit RG need *2 */
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3448_DP_TRANS_4P + val * 6 * 2,
			data[0] | data[1], GENMASK(15, 0));
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_344C_DP_TRANS_4P + val * 6 * 2,
			data[2] | data[3], GENMASK(15, 0));
	mtk_edp_autotest_update_bits(mtk_edp_test, REG_3450_DP_TRANS_4P + val * 6 * 2,
			data[4], GENMASK(15, 0));
}

static void mtk_edp_compliance_eye_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3478_DP_TRANS_4P,
							BIT(0), REG_CP2520_PATTERN1_DP_TRANS_4P);
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3478_DP_TRANS_4P,
							(0), REG_CP2520_PATTERN1_DP_TRANS_4P);
}

static void mtk_edp_select_cp2520_pattern2_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3478_DP_TRANS_4P,
							BIT(1), REG_CP2520_PATTERN2_DP_TRANS_4P);
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, REG_3478_DP_TRANS_4P,
							(0), REG_CP2520_PATTERN2_DP_TRANS_4P);
}

static void mtk_edp_select_cp2520_pattern3_enable(struct dp_cts_auto_req *mtk_edp_test, bool enable)
{
	/* CP2520 pattern3 is TPS4 */
	if (enable)
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_3400,
							BIT(15), PATTERN4_EN_DP_TRANS_P0_MASK);
	else
		mtk_edp_autotest_update_bits(mtk_edp_test, MTK_DP_TRANS_P0_3400,
							(0), PATTERN4_EN_DP_TRANS_P0_MASK);
}

static void mtk_edp_handle_test_pattern(struct dp_cts_auto_req *mtk_edp_test, u8 dpcd_248)
{
	u8 i = 0x0;
	u8 dpcd_tmp = 0x0;
	u8 PGdata[5] = {0x1F, 0x7C, 0xF0, 0xC1, 0x07};

	/* only support HBR3, HBR2, HBR, RBR bit rates */
	switch (dpcd_248 & LINK_QUAL_PATTERN_SELECT_MASK) {
	case PATTERN_NONE:
		pr_info("%s set PATTERN_NONE\n", EDP_PHY_AUTOTEST_DEBUG);
		/* Disable U02 patch code, add interskew for test pattern, */
		/* lane1/2/3 select lane0 pipe delay */
		break;
	case PATTERN_D10_2:
		pr_info("%s set PATTERN_D10_2\n", EDP_PHY_AUTOTEST_DEBUG);
		mtk_edp_set_training_pattern(mtk_edp_test, DPTX_TPS1);
		mtk_edp_phy_set_lane_pwr(mtk_edp_test, mtk_edp_test->ubLinkLaneCount);
		mtk_edp_set_lanes(mtk_edp_test, mtk_edp_test->ubLinkLaneCount >> 1);
		mdelay(2);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case PATTERN_SYMBOL_ERR:
		pr_info("%s set PATTERN_SYMBOL_ERR\n", EDP_PHY_AUTOTEST_DEBUG);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case PATTERN_PRBS7:
		pr_info("%s set PATTERN_PRBS7\n", EDP_PHY_AUTOTEST_DEBUG);
		mtk_edp_pattern_enable(mtk_edp_test, true);
		mtk_edp_prbs_enable(mtk_edp_test, true);
		mtk_edp_pattern_select(mtk_edp_test, DPTX_PG_PRBS7);
		mtk_edp_lane_pn_swap(mtk_edp_test, false);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case PATTERN_80B:
		pr_info("%s set PATTERN_80B\n", EDP_PHY_AUTOTEST_DEBUG);
		for (i = 0; i < 4; i++)
			mtk_edp_set_program_pattern(mtk_edp_test, i, PGdata);

		mtk_edp_pattern_enable(mtk_edp_test, true);
		mtk_edp_pattern_select(mtk_edp_test, DPTX_PG_80BIT);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case PATTERN_HBR2_COM_EYE:
		/* CP2520_PATTERN1 */
		pr_info("%s set PATTERN_HBR2_COM_EYE\n", EDP_PHY_AUTOTEST_DEBUG);
		mtk_edp_compliance_eye_enable(mtk_edp_test, true);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case CP2520_PATTERN2:
		pr_info("%s set CP2520_PATTERN2\n", EDP_PHY_AUTOTEST_DEBUG);
		mtk_edp_select_cp2520_pattern2_enable(mtk_edp_test, true);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	case CP2520_PATTERN3:
		pr_info("%s set CP2520_PATTERN3\n", EDP_PHY_AUTOTEST_DEBUG);
		dpcd_tmp = 0x01;
		mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE,
							&dpcd_tmp, sizeof(dpcd_tmp));
		break;
	default:
		break;
	}
}

static void mtk_edp_link_test_pattern(struct dp_cts_auto_req *mtk_edp_test)
{
	u8 dpcd_248 = 0x0;
	u8 lane_adjust[2] = {0x0};

	pr_info("%s LINK_TEST_PATTERN Start\n", EDP_PHY_AUTOTEST_DEBUG);
	mtk_edp_aux_read_access(mtk_edp_test, DP_PHY_TEST_PATTERN,
					&dpcd_248, sizeof(dpcd_248));
	pr_info("%s DPCD_248 = 0x%x", EDP_PHY_AUTOTEST_DEBUG, dpcd_248);

	mtk_edp_aux_read_access(mtk_edp_test, DP_ADJUST_REQUEST_LANE0_1,
						lane_adjust, sizeof(lane_adjust));

	mtk_edp_train_update_swing_pre(mtk_edp_test, mtk_edp_test->ubLinkRate,
					lane_adjust);

	mtk_edp_handle_test_pattern(mtk_edp_test, dpcd_248);
}

bool mtk_edp_phy_auto_test(struct dp_cts_auto_req *mtk_edp_test, u8 dpcd_201)
{
	u8 dpcd_218 = 0x0;
	u8 test_link_rate;
	u8 test_lane_count;
	u8 dpcd_tmp = 0x0;

	pr_info("%s %s+\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	pr_info("%s eDP PHY AutoTest Start\n", EDP_PHY_AUTOTEST_DEBUG);
	pr_info("%s DPCD_201 = 0x%x\n", EDP_PHY_AUTOTEST_DEBUG, dpcd_201);

	mtk_edp_aux_read_access(mtk_edp_test, DP_TEST_LINK_RATE, &test_link_rate, 0x1);
	mtk_edp_aux_read_access(mtk_edp_test, DP_TEST_LANE_COUNT, &test_lane_count, 0x1);
	mtk_edp_test->ubLinkLaneCount = test_lane_count;
	mtk_edp_test->ubLinkRate = test_link_rate;

	/* Device Service IRQ Vector for AUTOMATED TEST REQUEST */
	if (dpcd_201 & AUTOMATED_TEST_REQUEST) {
		mtk_edp_aux_write_access(mtk_edp_test, DP_DEVICE_SERVICE_IRQ_VECTOR, &dpcd_tmp, 0x1);
		usleep_range(1000, 2000);
		mtk_edp_aux_read_access(mtk_edp_test, DP_TEST_REQUEST, &dpcd_218, 0x1);
		pr_info("%s DPCD_218 = 0x%x\n", EDP_PHY_AUTOTEST_DEBUG, dpcd_218);

		switch (dpcd_218 & TEST_REQUEST_MASK) {
		case TEST_LINK_TRAINING:
			mtk_edp_test->bDPTxAutoTest_EN = TRUE;
			dpcd_tmp = 0x1;
			mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE, &dpcd_tmp,
								sizeof(dpcd_tmp));
			mtk_edp_test_link_training(mtk_edp_test);
			break;
		case TEST_VIDEO_PATTERN_REQUESTED:
			pr_info("%s TEST_VIDEO_PATTERN_REQUESTED Start\n", EDP_PHY_AUTOTEST_DEBUG);
			mtk_edp_test->bDPTxAutoTest_EN = TRUE;
			dpcd_tmp = 0x1;
			mtk_edp_aux_write_access(mtk_edp_test, DP_TEST_RESPONSE, &dpcd_tmp,
								sizeof(dpcd_tmp));
			mtk_edp_test_video_pattern(mtk_edp_test);
			break;
		case TEST_EDID_READ:
			mtk_edp_test_edid_read(mtk_edp_test);
			break;
		case LINK_TEST_PATTERN:
			mtk_edp_link_test_pattern(mtk_edp_test);
			break;
		case PHY_TEST_CHANNEL_CODING_TYPE:
			pr_info("%s PHY_TEST_CHANNEL_CODING_TYPE Start\n", EDP_PHY_AUTOTEST_DEBUG);
			pr_info("%s eDP noly suuport 8b/10b coding\n", EDP_PHY_AUTOTEST_DEBUG);
			break;
		case TEST_AUDIO_PATTERN_REQUESTED:
			pr_info("%s TEST_AUDIO_PATTERN_REQUESTED Start\n", EDP_PHY_AUTOTEST_DEBUG);
			pr_info("%s eDP not suuport audio\n", EDP_PHY_AUTOTEST_DEBUG);
			break;
		case TEST_AUDIO_DISABLED_VIDEO:
			pr_info("%s TEST_AUDIO_DISABLED_VIDEO Start\n", EDP_PHY_AUTOTEST_DEBUG);
			pr_info("%s eDP not suuport audio\n", EDP_PHY_AUTOTEST_DEBUG);
			break;
		default:
			pr_info("%s DPCD_218 not support\n", EDP_PHY_AUTOTEST_DEBUG);
			return FALSE;
		}
	}

	pr_info("%s %s-\n", EDP_PHY_AUTOTEST_DEBUG, __func__);
	return true;
}
EXPORT_SYMBOL(mtk_edp_phy_auto_test);

MODULE_AUTHOR("Jie-h.Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek Embedded DisplayPort PHY Auto Test Driver");
MODULE_LICENSE("GPL");
