// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <inc/engine_regs.h>
#include <inc/engine_gears.h>

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

/* Disable clk mux for engine (can be called in atomic context) */
void engine_gear_disable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	spin_lock(&gear_ctrl->lock);

	BUG_ON(gear_ctrl->clk_usage == 0);

	/* Sequence: power off -> disable clock */
	if (--gear_ctrl->clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_enc_wait_idle(ctrl);
		engine_dec_wait_idle(ctrl);

		/* IRQ is not available now */
		engine_set_irq_off(ctrl);

		/* Try to power off HW engine */
		if (engine_power_efficiency_enabled()) {
			engine_power_off(ctrl);
			gear_ctrl->power_on = false;
		}

		/* It's safe to disable clock now */
		clk_disable(gear_ctrl->clk_mux);
	}

	spin_unlock(&gear_ctrl->lock);
}

/* Disable clk mux for engine by cnt (can be called in atomic context) */
void engine_gear_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt)
{
	spin_lock(&gear_ctrl->lock);

	BUG_ON(gear_ctrl->clk_usage < cnt);

	gear_ctrl->clk_usage -= cnt;

	/* Sequence: power off -> disable clock */
	if (gear_ctrl->clk_usage == 0) {

		/* Wait for HW lat_fifo empty */
		engine_enc_wait_idle(ctrl);
		engine_dec_wait_idle(ctrl);

		/* IRQ is not available now */
		engine_set_irq_off(ctrl);

		/* Try to power off HW engine */
		if (engine_power_efficiency_enabled()) {
			engine_power_off(ctrl);
			gear_ctrl->power_on = false;
		}

		/* It's safe to disable clock now */
		clk_disable(gear_ctrl->clk_mux);
	}

	spin_unlock(&gear_ctrl->lock);
}

/* Return 0 if it's successful to gear up */
bool engine_try_to_gear_up(struct engine_gear_control_t *gear_ctrl, bool enc_wish)
{
	uint32_t new_gear;
	int ret;
	bool successful_gear_up = false;

	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_fixed)
		goto pre_exit;

	if (gear_ctrl->engine_gear_in_change)
		goto pre_exit;

	/*
	 * If it's max, just return.
	 * It may race with other requests and no gear setup is finished
	 * here. But at least someone is doing gear-up for us.
	 */
	if (gear_ctrl->curr_gear == ENGINE_MAX_GEAR) {
		new_gear = ENGINE_MAX_GEAR;
		goto exit;
	}

	/* Query new gear level */
	new_gear = gear_ctrl->curr_gear + 1;

	/* Gear is in change */
	gear_ctrl->engine_gear_in_change = true;
	spin_unlock(&gear_ctrl->lock);

	/* Now change voltage & clk to the higher one (May sleep). */
	ret = engine_setup_gear(gear_ctrl, new_gear, true);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, new_gear, ret);
	else
		pr_info("%s: new gear is %u.\n", __func__, new_gear);

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

pre_exit:
	spin_unlock(&gear_ctrl->lock);
	return successful_gear_up;
}

void engine_try_to_gear_down(struct engine_gear_control_t *gear_ctrl, bool enc_wish)
{
	uint32_t new_gear;
	int ret;

	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_fixed)
		goto pre_exit;

	if (gear_ctrl->engine_gear_in_change)
		goto pre_exit;

	/*
	 * If it's min, just return.
	 * It may race with other requests and no gear setup is finished
	 * here. But at least someone is doing gear-up for us.
	 */
	if (gear_ctrl->curr_gear == ENGINE_MIN_GEAR) {
		new_gear = ENGINE_MIN_GEAR;
		goto exit;
	}

	/* Query new gear level */
	new_gear = gear_ctrl->curr_gear - 1;
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

	/* Now change clk & voltage to the higher one (May sleep). */
	ret = engine_setup_gear(gear_ctrl, new_gear, false);
	if (ret)
		pr_info("%s: failed to set new gear %u: ret(%d).\n", __func__, new_gear, ret);
	else
		pr_info("%s: new gear is %u.\n", __func__, new_gear);

	/* Gear change is finished */
	spin_lock(&gear_ctrl->lock);

	/* Change to new gear successfully */
	if (!ret)
		gear_ctrl->curr_gear = new_gear;

	/* Gear is NOT in change */
	gear_ctrl->engine_gear_in_change = false;

exit:
	/* Update enc or dec wish gear */
	if (enc_wish)
		gear_ctrl->enc_wish_gear = new_gear;
	else
		gear_ctrl->dec_wish_gear = new_gear;

pre_exit:
	spin_unlock(&gear_ctrl->lock);
}

void engine_fix_gear_level(struct engine_gear_control_t *gear_ctrl, uint32_t gear_level)
{
	bool gear_up = false;
	int ret;

	if (gear_level > ENGINE_MAX_GEAR) {
		pr_info("%s: invalid gear:(%u)!\n", __func__, gear_level);
		return;
	}

	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_in_change) {
		pr_info("%s: gear is in change!\n", __func__);
		goto exit;
	}

	/* Gear is fixed */
	gear_ctrl->engine_gear_fixed = true;

	if (gear_ctrl->curr_gear == gear_level) {
		pr_info("%s: gear is identical:(%u)!\n", __func__, gear_level);
		goto exit;
	} else if (gear_ctrl->curr_gear < gear_level) {
		pr_info("%s: gear up to (%u)!\n", __func__, gear_level);
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

/* Engine gear will not be changed if gear_level == ENGINE_FREE_RUN_GEAR */
void engine_free_gear_level(struct engine_gear_control_t *gear_ctrl, uint32_t gear_level)
{
	bool gear_up;
	int ret;

	if (gear_level > ENGINE_MAX_GEAR && gear_level != ENGINE_FREE_RUN_GEAR) {
		pr_info("%s: invalid gear:(%u)!\n", __func__, gear_level);
		return;
	}

	spin_lock(&gear_ctrl->lock);

	if (gear_ctrl->engine_gear_in_change) {
		pr_info("%s: gear is in change!\n", __func__);
		goto exit;
	}

	/* Gear is freed */
	gear_ctrl->engine_gear_fixed = false;

	if (gear_level == ENGINE_FREE_RUN_GEAR)
		gear_level = gear_ctrl->curr_gear;

	if (gear_ctrl->curr_gear == gear_level) {
		pr_info("%s: gear is identical:(%u)!\n", __func__, gear_level);
		goto exit;
	} else if (gear_ctrl->curr_gear < gear_level) {
		pr_info("%s: gear up to (%u)!\n", __func__, gear_level);
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

	copied += snprintf(buf + copied, PAGE_SIZE - copied, "clk_usage: %u\n",
			gear_ctrl->clk_usage);
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "enc_wish_gear: %u\n",
			gear_ctrl->enc_wish_gear);
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "dec_wish_gear: %u\n",
			gear_ctrl->dec_wish_gear);
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "curr_gear: %u\n",
			gear_ctrl->curr_gear);
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "in-change(%s) fixed(%s)\n",
			gear_ctrl->engine_gear_in_change? "Y": "N",
			gear_ctrl->engine_gear_fixed? "Y" : "N");
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "PE(%s)\n",
			engine_power_efficiency_enabled()? "enabled" : "disabled");
	copied += snprintf(buf + copied, PAGE_SIZE - copied, "power(%s)\n",
			gear_ctrl->power_on? "on" : "off");

	/* Query clk & voltage (TODO) */

	spin_unlock(&gear_ctrl->lock);
	return copied;
}
