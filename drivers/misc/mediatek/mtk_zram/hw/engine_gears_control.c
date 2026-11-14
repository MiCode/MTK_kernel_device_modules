// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <inc/engine_regs.h>
#include <inc/engine_gears.h>
#include <inc/helpers.h>

/* Return 0 if success */
static int engine_setup_gear(struct engine_gear_control_t *gear_ctrl, uint32_t level, bool gear_up)
{
	int ret;

	if (gear_up) {

		/*
		 * Now change voltage & clk to the higher one (May sleep).
		 * (Update voltage first)
		 */
		ret = regulator_set_voltage(gear_ctrl->vcore, gear_ctrl->volt[level], INT_MAX);
		if (ret) {
			pr_info("%s: failed to set voltage to %d: ret(%d).\n",
					__func__, gear_ctrl->volt[level], ret);
			goto exit;
		}

		ret = clk_set_parent(gear_ctrl->clk_mux, gear_ctrl->clk_pll[level]);
		if (ret)
			pr_info("%s: failed to set clock to %u: ret(%d).\n", __func__, level, ret);
	} else {

		/*
		 * Now change clk & voltage to the lower one (May sleep).
		 * (Update clk first)
		 */
		ret = clk_set_parent(gear_ctrl->clk_mux, gear_ctrl->clk_pll[level]);
		if (ret) {
			pr_info("%s: failed to set clock to %u: ret(%d).\n", __func__, level, ret);
			goto exit;
		}

		ret = regulator_set_voltage(gear_ctrl->vcore, gear_ctrl->volt[level], INT_MAX);
		if (ret) {
			pr_info("%s: failed to set voltage to %d: ret(%d).\n",
					__func__, gear_ctrl->volt[level], ret);
		}
	}

exit:
	return ret;
}

void engine_gear_deinit(struct platform_device *pdev, struct engine_gear_control_t *gear_ctrl)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);

	/* Set up to min gear */
	WARN_ON(engine_setup_gear(gear_ctrl, ENGINE_MIN_GEAR, false));

	/* Undo preparation zram CG */
	clk_unprepare(gear_ctrl->clk_mux);
}

int engine_gear_init(struct platform_device *pdev, struct engine_gear_control_t *gear_ctrl)
{
	struct device *dev = &pdev->dev;
	char name[8];
	int i, ret;
	uint32_t volts[ENGINE_MAX_GEARS];

	/***********************/
	/* Set up clock source */
	/***********************/

	gear_ctrl->clk_mux = devm_clk_get(dev, "source");
	if (IS_ERR(gear_ctrl->clk_mux)) {
		pr_info("%s: failed to get clock.\n", __func__);
		return -ENOENT;
	}

	gear_ctrl->vcore = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(gear_ctrl->vcore)) {
		pr_info("%s: failed to get regulator.\n", __func__);
		return -ENOENT;
	}

	/* Query gear levels */
	for (i = 0; i < ENGINE_MAX_GEARS; i++) {
		snprintf(name, 8, "gear_%d", (i + ENGINE_GEAR_DTS_BASE));
		gear_ctrl->clk_pll[i] = devm_clk_get(dev, name);
		if (IS_ERR(gear_ctrl->clk_pll[i])) {
			pr_info("%s: failed to get clock pll[%d]:[%s].\n", __func__, i, name);
			return -ENOENT;
		}
	}

	/* Query corresponding vcore volts */
	if (of_property_read_u32_array(dev->of_node, "vcore-volts", volts, ENGINE_MAX_GEARS)) {
		pr_info("%s: failed to get vcore-volts.\n", __func__);
		return -ENOENT;
	}
	for (i = 0; i < ENGINE_MAX_GEARS; i++) {
		pr_info("%s: volt %u\n", __func__, volts[i]);
		gear_ctrl->volt[i] = volts[i];
	}

	/* Prepare a clock source */
	ret = clk_prepare(gear_ctrl->clk_mux);
	if (ret) {
		pr_info("%s: failed to prepare clock.\n", __func__);
		return ret;
	}

	/* Set up to min gear */
	ret = engine_setup_gear(gear_ctrl, ENGINE_MIN_GEAR, false);
	if (ret) {
		pr_info("%s: failed to set min gear.\n", __func__);
		return ret;
	}

	/* Initialize gear status for gear controls */
	gear_ctrl->clk_usage = 0;
	gear_ctrl->enc_wish_gear = ENGINE_MIN_GEAR;
	gear_ctrl->dec_wish_gear = ENGINE_MIN_GEAR;
	gear_ctrl->curr_gear = ENGINE_MIN_GEAR;
	gear_ctrl->engine_gear_in_change = false;
	gear_ctrl->engine_gear_fixed = false;
	gear_ctrl->power_on = false;
	spin_lock_init(&gear_ctrl->lock);

	return 0;
}

/*
 * Disable clk mux for engine by cnt (can be called in atomic context)
 * Should be called under gear_ctrl->lock acquired.
 */
static inline void __engine_gear_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt)
{
	/* It's forbidden to make clk_usage underflow */
	if (gear_ctrl->clk_usage < cnt)
		return;

	gear_ctrl->clk_usage -= cnt;

	/* Sequence: power off -> disable clock */
	if (gear_ctrl->clk_usage == 0) {

		/* Try to power off HW engine */
		if (engine_power_efficiency_enabled()) {
			engine_power_off(ctrl);
			gear_ctrl->power_on = false;
		}

		/* It's safe to disable clock now */
		clk_disable(gear_ctrl->clk_mux);
	}
}

/* Main entry to disable clks for compression (can be called in atomic context) */
void engine_gear_enc_disable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	spin_lock(&gear_ctrl->lock);

	/* It's forbidden to proceed when enc_clk_usage is 0 */
	if (gear_ctrl->enc_clk_usage == 0)
		goto exit;

	if (--gear_ctrl->enc_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_enc_wait_idle(ctrl);

		/* IRQ is not available for compression now */
		engine_set_irq_off(ctrl, true, false);

		/* Disable partial clk for compression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_ENC);
	}

	/* Sequence: disable partial clk -> disable clk mux */
	__engine_gear_disable_clock_by_cnt(ctrl, gear_ctrl, 1);

exit:
	spin_unlock(&gear_ctrl->lock);
}

/* Main entry to disable clks for decompression (can be called in atomic context) */
void engine_gear_dec_disable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	spin_lock(&gear_ctrl->lock);

	/* It's forbidden to proceed when dec_clk_usage is 0 */
	if (gear_ctrl->dec_clk_usage == 0)
		goto exit;

	if (--gear_ctrl->dec_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_dec_wait_idle(ctrl);

		/* IRQ is not available for decompression now */
		engine_set_irq_off(ctrl, false, true);

		/* Disable partial clk for decompression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_DEC);
	}

	/* Sequence: disable partial clk -> disable clk mux */
	__engine_gear_disable_clock_by_cnt(ctrl, gear_ctrl, 1);

exit:
	spin_unlock(&gear_ctrl->lock);
}

/*
 * Main entry to disable clks for engine (can be called in atomic context)
 * Should be paired with engine_gear_enable_clock.
 */
void engine_gear_disable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	spin_lock(&gear_ctrl->lock);

	/* It's forbidden to proceed when enc_clk_usage or dec_clk_usage is 0 */
	if (gear_ctrl->enc_clk_usage == 0 || gear_ctrl->dec_clk_usage == 0)
		goto exit;

	if (--gear_ctrl->enc_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_enc_wait_idle(ctrl);

		/* IRQ is not available for compression now */
		engine_set_irq_off(ctrl, true, false);

		/* Disable partial clk for compression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_ENC);
	}

	if (--gear_ctrl->dec_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_dec_wait_idle(ctrl);

		/* IRQ is not available for decompression now */
		engine_set_irq_off(ctrl, false, true);

		/* Disable partial clk for decompression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_DEC);
	}

	/* Sequence: disable partial clk -> disable clk mux */
	__engine_gear_disable_clock_by_cnt(ctrl, gear_ctrl, 2);

exit:
	spin_unlock(&gear_ctrl->lock);
}

/*
 * Main entry to disable clks for compression by cnt (can be called in atomic context)
 * Used by post process batch counting and enable IRQ if necessary.
 * Should be paired with engine_gear_enc_enable_clock_disable_irq.
 */
void engine_gear_enc_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt)
{
	spin_lock(&gear_ctrl->lock);

	/* It's forbidden to make enc_clk_usage underflow */
	if (gear_ctrl->enc_clk_usage < cnt)
		goto exit;

	gear_ctrl->enc_clk_usage -= cnt;

	if (gear_ctrl->enc_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_enc_wait_idle(ctrl);

		/* IRQ is not available for compression now */
		engine_set_irq_off(ctrl, true, false);

		/* Disable partial clk for compression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_ENC);

	} else {
		/* IRQ is restored to be available for compression now */
		engine_set_irq_on(ctrl, true, false);
	}

	/* Sequence: disable partial clk -> disable clk mux */
	__engine_gear_disable_clock_by_cnt(ctrl, gear_ctrl, cnt);

exit:
	spin_unlock(&gear_ctrl->lock);
}

/*
 * Main entry to disable clks for decompression by cnt (can be called in atomic context)
 * Used by post process batch counting and enable IRQ if necessary.
 * Should be paired with engine_gear_dec_enable_clock_disable_irq.
 */
void engine_gear_dec_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt)
{
	spin_lock(&gear_ctrl->lock);

	/* It's forbidden to make dec_clk_usage underflow */
	if (gear_ctrl->dec_clk_usage < cnt)
		goto exit;

	gear_ctrl->dec_clk_usage -= cnt;

	if (gear_ctrl->dec_clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_dec_wait_idle(ctrl);

		/* IRQ is not available for decompression now */
		engine_set_irq_off(ctrl, false, true);

		/* Disable partial clk for decompression */
		engine_clock_partial_disable(ctrl, ENGINE_CLK_DEC);

	} else {
		/* IRQ is restored to be available for decompression now */
		engine_set_irq_on(ctrl, false, true);
	}

	/* Sequence: disable partial clk -> disable clk mux */
	__engine_gear_disable_clock_by_cnt(ctrl, gear_ctrl, cnt);

exit:
	spin_unlock(&gear_ctrl->lock);
}

/*
 * Return true if it's successful to gear up.
 */
bool engine_try_to_gear_up(struct engine_gear_control_t *gear_ctrl, bool enc_wish)
{
	uint32_t new_gear = ENGINE_MAX_GEAR;
	int ret;
	bool successful_gear_up = false;

	spin_lock(&gear_ctrl->lock);

	/*
	 * Whether gear is in change.
	 * Someone is changing gear, and it may be set to lower level.
	 * It's ok because caller may retry again if it needs.
	 */
	if (gear_ctrl->engine_gear_in_change)
		goto exit;

	/*
	 * If it's max, just return.
	 * It may race with other requests and no gear setup is finished
	 * here. But at least someone is doing gear-up for us.
	 */
	if (gear_ctrl->curr_gear == ENGINE_MAX_GEAR) {
		/* It's successful when curr_gear is equal to max */
		successful_gear_up = true;
		goto exit;
	}

	/*
	 * Whether gear is fixed.
	 * The gear will be allowed to update when it's free run, and it will
	 * be set to proper gear level according enc and dec wish level.
	 */
	if (gear_ctrl->engine_gear_fixed) {
		/* It's unable to change gear level. View it as successful. */
		successful_gear_up = true;
		goto exit;
	}

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now change voltage & clk to the higher one (May sleep). */
	ret = engine_setup_gear(gear_ctrl, new_gear, true);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, new_gear, ret);
#ifdef ZRAM_ENGINE_DEBUG
	else
		pr_info("%s: new gear is %u.\n", __func__, new_gear);
#endif

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret) {
		gear_ctrl->curr_gear = new_gear;
		successful_gear_up = true;
	}

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;

exit:
	/* Update enc or dec wish gear */
	if (enc_wish)
		gear_ctrl->enc_wish_gear = new_gear;
	else
		gear_ctrl->dec_wish_gear = new_gear;

	spin_unlock(&gear_ctrl->lock);
	return successful_gear_up;
}

/*
 * Return true if the "wish" gear level is set to min.
 */
bool engine_try_to_gear_down(struct engine_gear_control_t *gear_ctrl, bool enc_wish)
{
	uint32_t new_gear;
	int ret;
	bool wish_gear_is_min = false;

	spin_lock(&gear_ctrl->lock);

	/*
	 * Whether gear is in change.
	 * Someone is changing gear, and it may be set to higher level.
	 * It's ok because caller may retry again if it needs.
	 */
	if (gear_ctrl->engine_gear_in_change)
		goto exit;

	/* Check wish gear level */
	if (enc_wish) {

		/* Reach the min wish gear level */
		if (gear_ctrl->enc_wish_gear == ENGINE_MIN_GEAR) {
			wish_gear_is_min = true;
			goto exit;
		}

		/* Decrease enc wish gear */
		gear_ctrl->enc_wish_gear--;

	} else {

		/* Reach the min wish gear level */
		if (gear_ctrl->dec_wish_gear == ENGINE_MIN_GEAR) {
			wish_gear_is_min = true;
			goto exit;
		}

		/* Decrease dec wish gear */
		gear_ctrl->dec_wish_gear--;
	}

	/*
	 * If it's min, just return.
	 * It may race with other requests and no gear setup is finished
	 * here. But at least someone is doing gear-up for us.
	 */
	if (gear_ctrl->curr_gear == ENGINE_MIN_GEAR)
		goto exit;

	/* Query new gear level */
	new_gear = gear_ctrl->curr_gear - 1;

	/*
	 * Whether gear is fixed.
	 * The gear will be allowed to update when it's free run, and it will
	 * be set to proper gear level according enc and dec wish level.
	 */
	if (gear_ctrl->engine_gear_fixed)
		goto exit;

	/* Whether new_gear is allowed */
	if (enc_wish) {
		if (new_gear < gear_ctrl->dec_wish_gear) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info("%s: Not permitted! new_gear %u is smaller than dec_wish %u.\n",
					__func__, new_gear, gear_ctrl->dec_wish_gear);
#endif
			goto exit;
		}
	} else {
		if (new_gear < gear_ctrl->enc_wish_gear) {
#ifdef ZRAM_ENGINE_DEBUG
			pr_info("%s: Not permitted! new_gear %u is smaller than enc_wish %u.\n",
					__func__, new_gear, gear_ctrl->enc_wish_gear);
#endif
			goto exit;
		}
	}

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now change clk & voltage to the lower one (May sleep). */
	ret = engine_setup_gear(gear_ctrl, new_gear, false);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, new_gear, ret);
#ifdef ZRAM_ENGINE_DEBUG
	else
		pr_info("%s: new gear is %u.\n", __func__, new_gear);
#endif

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret)
		gear_ctrl->curr_gear = new_gear;

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;

exit:
	spin_unlock(&gear_ctrl->lock);
	return wish_gear_is_min;
}

/*
 * Return true if it's successful to set gear to level or higher one.
 */
bool engine_try_to_set_gear_level(struct engine_gear_control_t *gear_ctrl, bool enc_wish, uint32_t level)
{
	uint32_t new_gear = level;
	int ret;
	bool successful_gear_set = false;

	spin_lock(&gear_ctrl->lock);

	/*
	 * Whether gear is in change.
	 * Someone is changing gear, and it may be set to lower level.
	 * It's ok because caller may retry again if it needs.
	 */
	if (gear_ctrl->engine_gear_in_change)
		goto exit;

	/*
	 * If it's already set to level, just return.
	 * It may race with other requests and no gear setup is finished
	 * here. But at least someone is doing gear-up for us.
	 */
	if (gear_ctrl->curr_gear >= new_gear) {
		/* It's successful when curr_gear is higher than or equal to new_gear */
		successful_gear_set = true;
		goto exit;
	}

	/*
	 * Whether gear is fixed.
	 * The gear will be allowed to update when it's free run, and it will
	 * be set to proper gear level according enc and dec wish level.
	 */
	if (gear_ctrl->engine_gear_fixed) {
		/* It's unable to change gear level. View it as successful. */
		successful_gear_set = true;
		goto exit;
	}

	/* Should we judge it with the other wish gear level (?) */

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now change voltage & clk to the higher one (May sleep). */
	ret = engine_setup_gear(gear_ctrl, new_gear, true);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, new_gear, ret);
#ifdef ZRAM_ENGINE_DEBUG
	else
		pr_info("%s: new gear is %u.\n", __func__, new_gear);
#endif

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret) {
		gear_ctrl->curr_gear = new_gear;
		successful_gear_set = true;
	}

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;

exit:
	/* Update enc or dec wish gear */
	if (enc_wish)
		gear_ctrl->enc_wish_gear = (new_gear > gear_ctrl->enc_wish_gear)? new_gear : gear_ctrl->enc_wish_gear;
	else
		gear_ctrl->dec_wish_gear = (new_gear > gear_ctrl->dec_wish_gear)? new_gear : gear_ctrl->dec_wish_gear;

	spin_unlock(&gear_ctrl->lock);
	return successful_gear_set;
}

void engine_fix_gear_level(struct engine_gear_control_t *gear_ctrl, uint32_t gear_level)
{
	bool gear_up = false;
	int ret;

	if (gear_level > ENGINE_MAX_GEAR) {
		pr_info("%s: invalid gear:(%u)!\n", __func__, gear_level);
		return;
	}

retry:
	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_in_change) {
		pr_info_ratelimited("%s: gear is in change!\n", __func__);

		/* Retry until we can fix the gear level */
		spin_unlock(&gear_ctrl->lock);
		goto retry;
	}

	/* Gear is fixed */
	gear_ctrl->engine_gear_fixed = true;

	if (gear_ctrl->curr_gear == gear_level) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: gear is identical:(%u)!\n", __func__, gear_level);
#endif
		goto exit;
	} else if (gear_ctrl->curr_gear < gear_level) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: gear up to (%u)!\n", __func__, gear_level);
#endif
		gear_up = true;
	}

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now set up to fix gear level (May sleep). */
	ret = engine_setup_gear(gear_ctrl, gear_level, gear_up);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, gear_level, ret);
	else
		pr_info("%s: gear is fixed to (%u).\n", __func__, gear_level);

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret)
		gear_ctrl->curr_gear = gear_level;

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;
exit:
	spin_unlock(&gear_ctrl->lock);
}

/* Free run engine gear (set to higher wish gear) */
void engine_free_gear_level(struct engine_gear_control_t *gear_ctrl)
{
	uint32_t gear_level;
	bool gear_up;
	int ret;

	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_in_change) {
		pr_info("%s: gear is in change!\n", __func__);
		goto exit;
	}

	/* Gear is freed */
	gear_ctrl->engine_gear_fixed = false;

	/* Choose gear_level for free run */
	if (gear_ctrl->enc_wish_gear > gear_ctrl->dec_wish_gear)
		gear_level = gear_ctrl->enc_wish_gear;
	else
		gear_level = gear_ctrl->dec_wish_gear;

	/* Whether we should change gear level */
	if (gear_ctrl->curr_gear == gear_level) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: gear is identical:(%u)!\n", __func__, gear_level);
#endif
		goto exit;
	} else if (gear_ctrl->curr_gear < gear_level) {
#ifdef ZRAM_ENGINE_DEBUG
		pr_info("%s: gear up to (%u)!\n", __func__, gear_level);
#endif
		gear_up = true;
	} else {
		pr_info("%s: gear down to (%u)!\n", __func__, gear_level);
		gear_up = false;
	}

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now set up to free gear level (May sleep). */
	ret = engine_setup_gear(gear_ctrl, gear_level, gear_up);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, gear_level, ret);
	else
		pr_info("%s: gear is freed to (%u).\n", __func__, gear_level);

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret)
		gear_ctrl->curr_gear = gear_level;

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;
exit:
	spin_unlock(&gear_ctrl->lock);
}

int engine_gear_get_status(struct engine_gear_control_t *gear_ctrl, char *buf)
{
	int copied = 0;

	spin_lock(&gear_ctrl->lock);

	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "clk_usage: %u(%u)(%u)\n",
			gear_ctrl->clk_usage, gear_ctrl->enc_clk_usage, gear_ctrl->dec_clk_usage);
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "enc_wish_gear: %u\n",
			gear_ctrl->enc_wish_gear);
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "dec_wish_gear: %u\n",
			gear_ctrl->dec_wish_gear);
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "curr_gear: %u\n",
			gear_ctrl->curr_gear);
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "in-change(%s) fixed(%s)\n",
			gear_ctrl->engine_gear_in_change? "Y": "N",
			gear_ctrl->engine_gear_fixed? "Y" : "N");
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "PE(%s)\n",
			engine_power_efficiency_enabled()? "enabled" : "disabled");
	ZRAM_DEBUG_DUMP(buf, copied, buf + copied, PAGE_SIZE - copied, "power(%s)\n",
			gear_ctrl->power_on? "on" : "off");

	spin_unlock(&gear_ctrl->lock);
	return copied;
}
