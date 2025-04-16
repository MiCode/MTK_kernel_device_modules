/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _ENGINE_GEARS_H_
#define _ENGINE_GEARS_H_

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/static_key.h>

#include <inc/engine_regs.h>

#define ENGINE_MAX_GEARS	(5)
#define ENGINE_GEAR_DTS_BASE	(3)	/* DTS -> Driver */
#define ENGINE_DEC_BASE_GEAR	(3)
#define ENGINE_ENC_BASE_GEAR	(0)
#define ENGINE_MAX_GEAR		(ENGINE_MAX_GEARS - 1)
#define ENGINE_MIN_GEAR		(0)
#define ENGINE_FREE_RUN_GEAR	(99)
#define ENGINE_ENABLE_GEAR_PE	(980)	/* Enable power efficiency */
#define ENGINE_DISABLE_GEAR_PE	(981)	/* Disable power efficiency */

/* Min start gear level for compression & decompression */
#define ENGINE_ENC_MIN_KICK_GEAR	(ENGINE_MIN_GEAR)
#define ENGINE_DEC_MIN_KICK_GEAR	(ENGINE_MAX_GEAR - 2)

struct engine_gear_control_t {

	/* gear status */
	uint32_t clk_usage;
	uint32_t enc_clk_usage;
	uint32_t dec_clk_usage;
	uint32_t enc_wish_gear;
	uint32_t dec_wish_gear;
	uint32_t curr_gear;			/* max(curr_enc_gear, curr_dec_gear) or manually */
	bool engine_gear_in_change;
	bool engine_gear_fixed;			/* curr_gear is fixed */
	bool power_on;

	/* Protect above status update */
	spinlock_t lock;

	/* gear sources */
	struct clk *clk_mux;			/* clk source */
	struct regulator *vcore;		/* voltage control */
	struct clk *clk_pll[ENGINE_MAX_GEARS];
	int volt[ENGINE_MAX_GEARS];

} ____cacheline_internodealigned_in_smp;

/* Static key control for power efficiency mode */
DECLARE_STATIC_KEY_TRUE(engine_power_efficiency);
static inline bool engine_power_efficiency_enabled(void)
{
	return static_branch_likely(&engine_power_efficiency);
}

int engine_gear_init(struct platform_device *pdev, struct engine_gear_control_t *gear_ctrl);
void engine_gear_deinit(struct platform_device *pdev, struct engine_gear_control_t *gear_ctrl);

/* Query usage count for clk mux */
static inline uint32_t engine_gear_clock_usage(struct engine_gear_control_t *gear_ctrl)
{
	uint32_t clk_usage;

	spin_lock(&gear_ctrl->lock);
	clk_usage = gear_ctrl->clk_usage;
	spin_unlock(&gear_ctrl->lock);

	return clk_usage;
}

/* A wrapper to safely call engine_power_on */
static inline int engine_gear_power_on(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = 0;

	spin_lock(&gear_ctrl->lock);

	/* Engine is powered on already, just return */
	if (gear_ctrl->power_on == true) {
		spin_unlock(&gear_ctrl->lock);
		return 0;
	}

	/* Enable clk for engine access */
	ret = clk_enable(gear_ctrl->clk_mux);
	if (!ret) {
		ret = engine_power_on(ctrl);
		if (!ret)
			gear_ctrl->power_on = true;

		/* No more access, disable clk */
		clk_disable(gear_ctrl->clk_mux);
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/* A wrapper to safely call engine_power_off */
static inline int engine_gear_power_off(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Engine is powered off already, just return */
	if (gear_ctrl->power_on == false) {
		spin_unlock(&gear_ctrl->lock);
		return 0;
	}

	/* It's allowed to power off engine when no more requests */
	if (gear_ctrl->clk_usage == 0) {

		/* Enable clk for engine access */
		ret = clk_enable(gear_ctrl->clk_mux);
		if (!ret) {
			/* Wait for HW lat_fifo empty */
			engine_enc_wait_idle(ctrl);
			engine_dec_wait_idle(ctrl);

			/* Ok. It's safe to power off engine */
			engine_power_off(ctrl);
			gear_ctrl->power_on = false;
			ret = 0;

			/* No more access, disable clk */
			clk_disable(gear_ctrl->clk_mux);
		}
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/*
 * Enable clk mux for engine by cnt (can be called in atomic context).
 * Should be called under gear_ctrl->lock acquired.
 */
static inline int __engine_gear_enable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt)
{
	int ret = 0;

	/* Sequence: enable clock -> power on */
	if (gear_ctrl->clk_usage == 0) {

		ret = clk_enable(gear_ctrl->clk_mux);

		if (engine_power_efficiency_enabled() && !ret) {
			ret = engine_power_on(ctrl);
			if (!ret)
				gear_ctrl->power_on = true;
		}
	}

	/* Increment the clk_usage without the check of ret (intentional) */
	gear_ctrl->clk_usage += cnt;

	return ret;
}

/* Main entry to enable clks for compression (can be called in atomic context) */
static inline int engine_gear_enc_enable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Sequence: enable clk mux -> enable partial clk */
	ret = __engine_gear_enable_clock_by_cnt(ctrl, gear_ctrl, 1);

	if (!ret && gear_ctrl->enc_clk_usage++ == 0) {

		/* Enable partial clk for compression */
		engine_clock_partial_enable(ctrl, ENGINE_CLK_ENC);

		/* IRQ is available for compression now */
		engine_set_irq_on(ctrl, true, false);
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/* Main entry to enable clks for decompression (can be called in atomic context) */
static inline int engine_gear_dec_enable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Sequence: enable clk mux -> enable partial clk */
	ret = __engine_gear_enable_clock_by_cnt(ctrl, gear_ctrl, 1);

	if (!ret && gear_ctrl->dec_clk_usage++ == 0) {

		/* Enable partial clk for decompression */
		engine_clock_partial_enable(ctrl, ENGINE_CLK_DEC);

		/* IRQ is available for decompression now */
		engine_set_irq_on(ctrl, false, true);
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/* Main entry to enable clks for engine (can be called in atomic context) */
static inline int engine_gear_enable_clock(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Sequence: enable clk mux -> enable partial clk */
	ret = __engine_gear_enable_clock_by_cnt(ctrl, gear_ctrl, 2);

	if (!ret) {
		/* Compression part */
		if (gear_ctrl->enc_clk_usage++ == 0) {

			/* Enable partial clk for compression */
			engine_clock_partial_enable(ctrl, ENGINE_CLK_ENC);

			/* IRQ is available for compression now */
			engine_set_irq_on(ctrl, true, false);
		}

		/* Decompression part */
		if (gear_ctrl->dec_clk_usage++ == 0) {

			/* Enable partial clk for decompression */
			engine_clock_partial_enable(ctrl, ENGINE_CLK_DEC);

			/* IRQ is available for decompression now */
			engine_set_irq_on(ctrl, false, true);
		}
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/*
 * Main entry to enable clks for compression (can be called in atomic context)
 * Used by post process to disable compression IRQ when it starts processing.
 */
static inline int engine_gear_enc_enable_clock_disable_irq(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Sequence: enable clk mux -> enable partial clk */
	ret = __engine_gear_enable_clock_by_cnt(ctrl, gear_ctrl, 1);

	if (!ret) {
		if (gear_ctrl->enc_clk_usage++ == 0) {
			/* Enable partial clk for compression */
			engine_clock_partial_enable(ctrl, ENGINE_CLK_ENC);
		} else {
			/* IRQ is not necessary for compression now */
			engine_set_irq_off(ctrl, true, false);
		}
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/*
 * Main entry to enable clks for decompression (can be called in atomic context)
 * Used by post process to disable decompression IRQ when it starts processing.
 */
static inline int engine_gear_dec_enable_clock_disable_irq(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl)
{
	int ret = -1;

	spin_lock(&gear_ctrl->lock);

	/* Sequence: enable clk mux -> enable partial clk */
	ret = __engine_gear_enable_clock_by_cnt(ctrl, gear_ctrl, 1);

	if (!ret) {
		if (gear_ctrl->dec_clk_usage++ == 0) {
			/* Enable partial clk for decompression */
			engine_clock_partial_enable(ctrl, ENGINE_CLK_DEC);
		} else {
			/* IRQ is not necessary for decompression now */
			engine_set_irq_off(ctrl, false, true);
		}
	}

	spin_unlock(&gear_ctrl->lock);

	return ret;
}

/* Disable clk mux for engine (can be called in atomic context) */
void engine_gear_enc_disable_clock(struct engine_control_t *ctrl, struct engine_gear_control_t *gear_ctrl);
void engine_gear_dec_disable_clock(struct engine_control_t *ctrl, struct engine_gear_control_t *gear_ctrl);
void engine_gear_disable_clock(struct engine_control_t *ctrl, struct engine_gear_control_t *gear_ctrl);
void engine_gear_enc_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt);
void engine_gear_dec_disable_clock_by_cnt(struct engine_control_t *ctrl,
		struct engine_gear_control_t *gear_ctrl, uint32_t cnt);

/* Gear up or down */
bool engine_try_to_gear_up(struct engine_gear_control_t *gear_ctrl, bool enc_wish);
bool engine_try_to_gear_down(struct engine_gear_control_t *gear_ctrl, bool enc_wish);

/* Set initial gear level */
bool engine_try_to_set_gear_level(struct engine_gear_control_t *gear_ctrl, bool enc_wish, uint32_t level);

/* Fix or free gear */
void engine_fix_gear_level(struct engine_gear_control_t *gear_ctrl, uint32_t level);
void engine_free_gear_level(struct engine_gear_control_t *gear_ctrl);

/* Dump gear status */
int engine_gear_get_status(struct engine_gear_control_t *gear_ctrl, char *buf);

#endif /* _ENGINE_GEARS_H_ */
