
// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/pinctrl/consumer.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <mt-plat/mtk_pwm.h>
#include <mt-plat/mtk_pwm_hal.h>
#include <mt-plat/mtk_pwm_hal_pub.h>
//#include <mach/mtk_pwm_prv.h>
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"
#include <linux/power_supply.h>
#endif
#ifndef OCP8135B_DTNAME
#define OCP8135B_DTNAME "mediatek,ocp8135b"
#endif
#define OCP8135B_NAME	"ocp8135b"
/* registers definitions */
/* TODO: define register */
#define OCP8135B_FLASH_BRT_MIN 25000
#define OCP8135B_FLASH_BRT_STEP 10750
#define OCP8135B_FLASH_BRT_MAX 1100000

#define OCP8135B_FLASH_TOUT_MIN 200
#define OCP8135B_FLASH_TOUT_STEP 200
#define OCP8135B_FLASH_TOUT_MAX 1600

#define OCP8135B_TORCH_BRT_MIN 25000
#define OCP8135B_TORCH_BRT_STEP 2350
#define OCP8135B_TORCH_BRT_MAX 395000
#define OCP8135B_TORCH_BRT_FAST 120000
/* struct ocp8135b_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct ocp8135b_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt;
	u32 max_torch_brt;
};
/**
 * struct ocp8135b_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct ocp8135b_flash {
	struct device *dev;
	struct ocp8135b_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led;
	struct v4l2_subdev subdev_led;
	struct pinctrl *ocp8135b_pinctrl;
	struct pinctrl_state *ocp8135b_enf_high;
	struct pinctrl_state *ocp8135b_enf_low;
	struct pinctrl_state *ocp8135b_enm_high;
	struct pinctrl_state *ocp8135b_enm_low;
	struct pinctrl_state *ocp8135b_enm_pwm;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id;
#endif
};
/* define usage count */
static int use_count;
static struct ocp8135b_flash *ocp8135b_flash_data;
static int ocp8135b_set_driver(int set);
#define to_ocp8135b_flash(_ctrl)	\
	container_of(_ctrl->handler, struct ocp8135b_flash, ctrls_led)
/* define pinctrl */
/* TODO: define pinctrl */
#define OCP8135B_PINCTRL_PIN_ENF 0
#define OCP8135B_PINCTRL_PIN_ENM 1
#define OCP8135B_PINCTRL_PINSTATE_LOW 0
#define OCP8135B_PINCTRL_PINSTATE_HIGH 1
#define OCP8135B_PINCTRL_PINSTATE_PWM 2
#define OCP8135B_PINCTRL_STATE_ENF_HIGH "enf-high"
#define OCP8135B_PINCTRL_STATE_ENF_LOW  "enf-low"
#define OCP8135B_PINCTRL_STATE_ENM_HIGH "enm-high"
#define OCP8135B_PINCTRL_STATE_ENM_LOW  "enm-low"
#define OCP8135B_PINCTRL_STATE_ENM_PWM  "enm-pwm"

static struct pwm_spec_config ocp8135b_pwm_config = {
	.pwm_no = 2,
	.mode = PWM_MODE_OLD,
	.clk_div = CLK_DIV4,
	.clk_src = PWM_CLK_OLD_MODE_BLOCK,
	.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_OLD_REGS.GDURATION = 0,
	.PWM_MODE_OLD_REGS.WAVE_NUM = 0,
	.PWM_MODE_OLD_REGS.DATA_WIDTH = 99,
	.PWM_MODE_OLD_REGS.THRESH = 49,
};

static unsigned int ocp8135b_timeout_ms = -1;
static unsigned char ocp8135b_pwm_worked = false;
static unsigned char ocp8135b_enf_state = OCP8135B_PINCTRL_PINSTATE_LOW;

static struct work_struct ocp8135b_work;
static struct hrtimer ocp8135b_timer;
static int flash_current = OCP8135B_TORCH_BRT_MIN;

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int ocp8135b_pinctrl_init(struct ocp8135b_flash *flash)
{
	int ret = 0;
	/* get pinctrl */
	flash->ocp8135b_pinctrl = devm_pinctrl_get(flash->dev);
	if (IS_ERR(flash->ocp8135b_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(flash->ocp8135b_pinctrl);
		return ret;
	}
	/* Flashlight HWEN pin initialization */
	flash->ocp8135b_enf_high = pinctrl_lookup_state(
			flash->ocp8135b_pinctrl,
			OCP8135B_PINCTRL_STATE_ENF_HIGH);
	if (IS_ERR(flash->ocp8135b_enf_high)) {
		pr_info("Failed to init (%s)\n",
			OCP8135B_PINCTRL_STATE_ENF_HIGH);
		ret = PTR_ERR(flash->ocp8135b_enf_high);
	}
	flash->ocp8135b_enf_low = pinctrl_lookup_state(
			flash->ocp8135b_pinctrl,
			OCP8135B_PINCTRL_STATE_ENF_LOW);
	if (IS_ERR(flash->ocp8135b_enf_low)) {
		pr_info("Failed to init (%s)\n", OCP8135B_PINCTRL_STATE_ENF_LOW);
		ret = PTR_ERR(flash->ocp8135b_enf_low);
	}

	flash->ocp8135b_enm_high = pinctrl_lookup_state(
			flash->ocp8135b_pinctrl,
			OCP8135B_PINCTRL_STATE_ENM_HIGH);
	if (IS_ERR(flash->ocp8135b_enm_high)) {
		pr_info("Failed to init (%s)\n",
			OCP8135B_PINCTRL_STATE_ENM_HIGH);
		ret = PTR_ERR(flash->ocp8135b_enm_high);
	}
	flash->ocp8135b_enm_low = pinctrl_lookup_state(
			flash->ocp8135b_pinctrl,
			OCP8135B_PINCTRL_STATE_ENM_LOW);
	if (IS_ERR(flash->ocp8135b_enm_low)) {
		pr_info("Failed to init (%s)\n", OCP8135B_PINCTRL_STATE_ENM_LOW);
		ret = PTR_ERR(flash->ocp8135b_enm_low);
	}

	flash->ocp8135b_enm_pwm = pinctrl_lookup_state(
			flash->ocp8135b_pinctrl,
			OCP8135B_PINCTRL_STATE_ENM_PWM);
	if (IS_ERR(flash->ocp8135b_enm_pwm)) {
		pr_info("Failed to init (%s)\n",
			OCP8135B_PINCTRL_STATE_ENM_PWM);
		ret = PTR_ERR(flash->ocp8135b_enm_pwm);
	}

	return ret;
}
static int ocp8135b_pinctrl_set(struct ocp8135b_flash *flash, int pin, int state)
{
	int ret = 0;
	if (IS_ERR(flash->ocp8135b_pinctrl)) {
		pr_info("pinctrl is not available\n");
		return -1;
	}
	switch (pin) {
	case OCP8135B_PINCTRL_PIN_ENF:
		if (state == OCP8135B_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(flash->ocp8135b_enf_low)){
			ret = pinctrl_select_state(flash->ocp8135b_pinctrl,
					flash->ocp8135b_enf_low);
			ocp8135b_enf_state = OCP8135B_PINCTRL_PINSTATE_LOW;
		}else if (state == OCP8135B_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(flash->ocp8135b_enf_high)){
			ret = pinctrl_select_state(flash->ocp8135b_pinctrl,
					flash->ocp8135b_enf_high);
			ocp8135b_enf_state = OCP8135B_PINCTRL_PINSTATE_HIGH;
		}else{
			pr_info("set err, pin(%d) state(%d)\n", pin, state);
		}
		break;
	case OCP8135B_PINCTRL_PIN_ENM:
		if (state == OCP8135B_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(flash->ocp8135b_enm_low))
			ret = pinctrl_select_state(flash->ocp8135b_pinctrl,
					flash->ocp8135b_enm_low);
		else if (state == OCP8135B_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(flash->ocp8135b_enm_high))
			ret = pinctrl_select_state(flash->ocp8135b_pinctrl,
					flash->ocp8135b_enm_high);
		else if (state == OCP8135B_PINCTRL_PINSTATE_PWM &&
				!IS_ERR(flash->ocp8135b_enm_pwm))
			ret = pinctrl_select_state(flash->ocp8135b_pinctrl,
					flash->ocp8135b_enm_pwm);
		else
			pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("[%s] pin(%d) state(%d)\n", __func__, pin, state);
	return ret;
}

static int ocp8135b_get_pwm_duty(const unsigned int max_Iq, const unsigned int brt) {
	int percent = 0;

	percent = 200 * brt / max_Iq;
	percent = percent < 0 ? 0 : percent;
	percent = percent > 199 ? 199 : percent;
	pr_info("[%s] flash percent:%d\n", __func__, percent);
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH = 199;
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.THRESH = percent;
	return 0;
}

static int ocp8135b_force_flash_duty(const unsigned int duty) {
	unsigned int percent = 0;

	percent = duty;
	percent = percent < 0 ? 0 : percent;
	percent = percent > 99 ? 99 : percent;
	pr_info("[%s] flash percent:%d\n", __func__, percent);
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH = 99;
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.THRESH = percent;
	return 0;
}

static int ocp8135b_force_torch_duty(const unsigned int duty) {
	unsigned int percent = 0;
	unsigned int brt = OCP8135B_TORCH_BRT_MIN;

	percent = duty;
	percent = percent < 0 ? 0 : percent;
	percent = percent > 100 ? 100 : percent;
	brt += (percent * OCP8135B_TORCH_BRT_STEP);
	pr_info("[%s] flash percent:%d, brt:%d\n", __func__, percent, brt);
	ocp8135b_get_pwm_duty(OCP8135B_TORCH_BRT_MAX, brt);
	return 0;
}

static int ocp8135b_pwm_enable(struct ocp8135b_flash *flash) {
	if(!ocp8135b_pwm_worked){
		pinctrl_select_state(flash->ocp8135b_pinctrl,
			flash->ocp8135b_enm_pwm);
		mt_pwm_clk_sel_hal(ocp8135b_pwm_config.pwm_no, CLK_26M);
	}
	pr_info("[%s] husf pwm_no(%d) thresh(%d)/data width(%d)\n", __func__,
			ocp8135b_pwm_config.pwm_no,
			ocp8135b_pwm_config.PWM_MODE_OLD_REGS.THRESH,
			ocp8135b_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH);
	pwm_set_spec_config(&ocp8135b_pwm_config);
	ocp8135b_pwm_worked = true;
	return 0;
}

static int ocp8135b_pwm_disable(void) {
	if(ocp8135b_pwm_worked){
		mt_pwm_disable(ocp8135b_pwm_config.pwm_no, false);
		ocp8135b_pwm_worked = false;
	}
	return 0;
}

static int ocp8135b_enm_high(struct ocp8135b_flash *flash) {
	int rval = 0;
	rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENM, OCP8135B_PINCTRL_PINSTATE_PWM);
	mt_pwm_clk_sel_hal(ocp8135b_pwm_config.pwm_no, CLK_26M);
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH = 199;
	ocp8135b_pwm_config.PWM_MODE_OLD_REGS.THRESH = 199;
	pwm_set_spec_config(&ocp8135b_pwm_config);
	ocp8135b_pwm_worked = true;
	pr_info("[%s] enm high by full pwm\n", __func__);
	return rval;
}

/* led1/2 enable/disable */
static int ocp8135b_enable_ctrl(struct ocp8135b_flash *flash, bool on)
{
	int rval = 0;
	/* TODO: wrap enable function */
	if (on) {
		rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_LOW);
	    rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENM, OCP8135B_PINCTRL_PINSTATE_HIGH);
		pr_info("[%s] on \n", __func__);
	} else {
	    rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENM, OCP8135B_PINCTRL_PINSTATE_LOW);
	    rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_LOW);
		ocp8135b_pwm_disable();
		pr_info("[%s] off \n", __func__);
	}
	return rval;
}

/* torch1/2 brightness control */
static int ocp8135b_torch_brt_ctrl(struct ocp8135b_flash *flash, unsigned int brt)
{
	int rval = 0;
	pr_info("[%s] brt = %d \n", __func__, brt);
	/* TODO: wrap set torch brightness function */
	if(brt <= OCP8135B_TORCH_BRT_MIN){
		ocp8135b_enable_ctrl(flash, false);
	} else {
		if(!ocp8135b_pwm_worked){
			rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_LOW);
			rval = ocp8135b_enm_high(flash);
			if(OCP8135B_TORCH_BRT_FAST < brt){
				udelay(10000);
			}else{
				udelay(6000);
			}
		}
		ocp8135b_get_pwm_duty(OCP8135B_TORCH_BRT_MAX, brt);
		ocp8135b_pwm_enable(flash);
	}

	return rval;
}
/* flash1/2 brightness control */
static int ocp8135b_flash_brt_ctrl(struct ocp8135b_flash *flash, unsigned int brt)
{
	pr_info("[%s] brt = %d \n", __func__, brt);
	/* TODO: wrap set flash brightness function */
	if(brt <= OCP8135B_FLASH_BRT_MIN){
		ocp8135b_enable_ctrl(flash, false);
		return 0;
	}
	flash_current = brt;
	return 0;
}

static int ocp8135b_flash_enable(struct ocp8135b_flash *flash)
{
	int rval = 0;
	pr_info("[%s] brt = %d \n", __func__, flash_current);
	/* TODO: wrap set flash brightness function */
	if(flash_current <= OCP8135B_FLASH_BRT_MIN){
		ocp8135b_enable_ctrl(flash, false);
		return 0;
	}
	pinctrl_select_state(flash->ocp8135b_pinctrl,
			flash->ocp8135b_enm_pwm);
	ocp8135b_get_pwm_duty(OCP8135B_FLASH_BRT_MAX, flash_current);
	pr_info("[%s] pwm_no(%d) thresh(%d)/data width(%d)\n", __func__,
			ocp8135b_pwm_config.pwm_no,
			ocp8135b_pwm_config.PWM_MODE_OLD_REGS.THRESH,
			ocp8135b_pwm_config.PWM_MODE_OLD_REGS.DATA_WIDTH);
	mt_pwm_clk_sel_hal(ocp8135b_pwm_config.pwm_no, CLK_26M);
	pwm_set_spec_config(&ocp8135b_pwm_config);
	mdelay(10);
	rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_HIGH);
	return rval;
}

/* enable mode control */
static int ocp8135b_mode_ctrl(struct ocp8135b_flash *flash)
{
	int rval = -EINVAL;
	/* TODO: wrap mode ctrl function */
	pr_info("[%s] mode(%d) \n", __func__, flash->led_mode);
	switch (flash->led_mode) {
	case V4L2_FLASH_LED_MODE_NONE:
	    rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENM, OCP8135B_PINCTRL_PINSTATE_LOW);
	    rval = ocp8135b_pinctrl_set(flash, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_LOW);
		ocp8135b_pwm_disable();
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
		ocp8135b_flash_enable(flash);
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
	default:
		break;
	}
	return rval;
}

/* flash1/2 timeout control */
static int ocp8135b_flash_tout_ctrl(struct ocp8135b_flash *flash,
				unsigned int tout)
{
	int rval = 0;
	/* TODO: wrap set flash timeout function */
	return rval;
}
/* v4l2 controls  */
static int ocp8135b_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ocp8135b_flash *flash = to_ocp8135b_flash(ctrl);
	int rval = -EINVAL;
	mutex_lock(&flash->lock);
	/* TODO: wrap get hw fault function */
	mutex_unlock(&flash->lock);
	return rval;
}
static int ocp8135b_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ocp8135b_flash *flash = to_ocp8135b_flash(ctrl);
	int rval = -EINVAL;
	mutex_lock(&flash->lock);
	pr_info("[%s] id: %x", __func__, ctrl->id);
	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		flash->led_mode = ctrl->val;
		pr_info("[%s] V4L2_CID_FLASH_LED_MODE(%d)", __func__, ctrl->val);
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = ocp8135b_mode_ctrl(flash);
		else
			rval = 0;
		if (flash->led_mode == V4L2_FLASH_LED_MODE_NONE)
			ocp8135b_enable_ctrl(flash, false);
		break;
	case V4L2_CID_FLASH_STROBE_SOURCE:
		break;
	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = ocp8135b_mode_ctrl(flash);
		break;
	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = ocp8135b_mode_ctrl(flash);
		ocp8135b_enable_ctrl(flash, false);
		break;
	case V4L2_CID_FLASH_TIMEOUT:
		rval = ocp8135b_flash_tout_ctrl(flash, ctrl->val);
		break;
	case V4L2_CID_FLASH_INTENSITY:
		rval = ocp8135b_flash_brt_ctrl(flash, ctrl->val);
		break;
	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = ocp8135b_torch_brt_ctrl(flash, ctrl->val);
		break;
	}
err_out:
	mutex_unlock(&flash->lock);
	return rval;
}
static int ocp8135b_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return ocp8135b_get_ctrl(ctrl);
}
static int ocp8135b_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return ocp8135b_set_ctrl(ctrl);
}
static const struct v4l2_ctrl_ops ocp8135b_led_ctrl_ops = {
		.g_volatile_ctrl = ocp8135b_led0_get_ctrl,
		.s_ctrl = ocp8135b_led0_set_ctrl,
};
static int ocp8135b_init_controls(struct ocp8135b_flash *flash)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt;
	u32 max_torch_brt = flash->pdata->max_torch_brt;
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led;
	const struct v4l2_ctrl_ops *ops = &ocp8135b_led_ctrl_ops;
	v4l2_ctrl_handler_init(hdl, 8);
	/* flash mode */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_LED_MODE,
			       V4L2_FLASH_LED_MODE_TORCH, ~0x7,
			       V4L2_FLASH_LED_MODE_NONE);
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
	/* flash source */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_STROBE_SOURCE,
			       0x1, ~0x3, V4L2_FLASH_STROBE_SOURCE_SOFTWARE);
	/* flash strobe */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE, 0, 0, 0, 0);
	/* flash strobe stop */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE_STOP, 0, 0, 0, 0);
	/* flash strobe timeout */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TIMEOUT,
			  OCP8135B_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  OCP8135B_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);
	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  OCP8135B_FLASH_BRT_MIN, max_flash_brt,
			  OCP8135B_FLASH_BRT_STEP, max_flash_brt);
	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  OCP8135B_TORCH_BRT_MIN, max_torch_brt,
			  OCP8135B_TORCH_BRT_STEP, max_torch_brt);
	/* fault */
	fault = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_FAULT, 0,
				  V4L2_FLASH_FAULT_OVER_VOLTAGE
				  | V4L2_FLASH_FAULT_OVER_TEMPERATURE
				  | V4L2_FLASH_FAULT_SHORT_CIRCUIT
				  | V4L2_FLASH_FAULT_TIMEOUT, 0, 0);
	if (fault != NULL)
		fault->flags |= V4L2_CTRL_FLAG_VOLATILE;
	if (hdl->error)
		return hdl->error;
	flash->subdev_led.ctrl_handler = hdl;
	return 0;
}
/* initialize device */
static const struct v4l2_subdev_ops ocp8135b_ops = {
	.core = NULL,
};
static const struct regmap_config ocp8135b_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};
static void ocp8135b_v4l2_i2c_subdev_init(struct v4l2_subdev *sd,
		const struct v4l2_subdev_ops *ops)
{
	int ret = 0;
	struct ocp8135b_flash *flash = container_of(sd, struct ocp8135b_flash, subdev_led);
	v4l2_subdev_init(sd, ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->owner = flash->dev->driver->owner;
	sd->dev = flash->dev;
	/* initialize name */
	ret = snprintf(sd->name, sizeof(sd->name), "%s 00-0001",
		flash->dev->driver->name);
	if (ret < 0)
		pr_info("snprintf failed\n");
}
static int ocp8135b_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s open\n", __func__);
	ocp8135b_set_driver(1);
	return 0;
}
static int ocp8135b_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s close\n", __func__);
	ocp8135b_set_driver(0);
	return 0;
}
static const struct v4l2_subdev_internal_ops ocp8135b_int_ops = {
	.open = ocp8135b_open,
	.close = ocp8135b_close,
};
static int ocp8135b_subdev_init(struct ocp8135b_flash *flash, char *led_name)
{
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;
	pr_info("[%s] + \n", __func__);
	ocp8135b_v4l2_i2c_subdev_init(&flash->subdev_led, &ocp8135b_ops);
	flash->subdev_led.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->subdev_led.internal_ops = &ocp8135b_int_ops;
	strscpy(flash->subdev_led.name, led_name,
		sizeof(flash->subdev_led.name));
	child = of_get_child_by_name(np, fled_name);
	if(child){
		pr_info("[%s] get child Succuss", __func__);
		flash->subdev_led.fwnode = of_fwnode_handle(child);
	}
	rval = ocp8135b_init_controls(flash);
	if (rval)
		goto err_out;
	rval = media_entity_pads_init(&flash->subdev_led.entity, 0, NULL);
	if (rval < 0)
		goto err_out;
	flash->subdev_led.entity.function = MEDIA_ENT_F_FLASH;
	rval = v4l2_async_register_subdev(&flash->subdev_led);
	if (rval < 0)
		goto err_out;
	pr_info("[%s] - \n", __func__);
	return rval;
err_out:
	v4l2_ctrl_handler_free(&flash->ctrls_led);
	return rval;
}
/* flashlight init */
static int ocp8135b_init(struct ocp8135b_flash *flash)
{
	int rval = 0;
	pr_info("[%s] \n", __func__);
	/* TODO: wrap init function */
	ocp8135b_enable_ctrl(flash, false);
	return rval;
}
/* flashlight uninit */
static int ocp8135b_uninit(struct ocp8135b_flash *flash)
{
	pr_info("[%s] \n", __func__);
	ocp8135b_enable_ctrl(flash, false);
	return 0;
}
static int ocp8135b_flash_open(void)
{
	return 0;
}
static int ocp8135b_flash_release(void)
{
	return 0;
}
static int ocp8135b_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	fl_arg = (struct flashlight_dev_arg *)arg;
	switch (cmd) {
	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF: %d\n", (int)fl_arg->arg);
		if ((int)fl_arg->arg) {
			ocp8135b_torch_brt_ctrl(ocp8135b_flash_data, 25000);
			ocp8135b_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
			ocp8135b_mode_ctrl(ocp8135b_flash_data);
		} else {
			ocp8135b_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
			ocp8135b_mode_ctrl(ocp8135b_flash_data);
			ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
		}
		break;
	case XIAOMI_FLASH_GET_HWINFO_STEP:
		fl_arg->arg = OCP8135B_TORCH_BRT_STEP;
		pr_info("XIAOMI_FLASH_GET_HWINFO_STEP: %d\n",
				(int)fl_arg->arg);
		break;
	case XIAOMI_FLASH_GET_HWINFO_MIN:
		fl_arg->arg = OCP8135B_TORCH_BRT_MIN;
		pr_info("XIAOMI_FLASH_GET_HWINFO_MIN: %d\n",
				(int)fl_arg->arg);
	break;
	default:
		pr_info("No such command and arg: (%d, %d)\n", _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}
	return 0;
}
static int ocp8135b_set_driver(int set)
{
	int ret = 0;
	/* set chip and usage count */
	//mutex_lock(&ocp8135b_mutex);
	if (set) {
		if (!use_count)
			ret = ocp8135b_init(ocp8135b_flash_data);
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = ocp8135b_uninit(ocp8135b_flash_data);
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	//mutex_unlock(&ocp8135b_mutex);
	return 0;
}
static ssize_t ocp8135b_strobe_store(struct flashlight_arg arg)
{
	pr_info("[%s] \n", __func__);
	ocp8135b_set_driver(1);
	//ocp8135b_set_level(arg.channel, arg.level);
	//ocp8135b_timeout_ms[arg.channel] = 0;
	//ocp8135b_enable(arg.channel);
	ocp8135b_torch_brt_ctrl(ocp8135b_flash_data, arg.level * 25000);
	ocp8135b_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
	ocp8135b_mode_ctrl(ocp8135b_flash_data);
	msleep(arg.dur);
	//ocp8135b_disable(arg.channel);
	ocp8135b_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
	ocp8135b_mode_ctrl(ocp8135b_flash_data);
	ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
	ocp8135b_set_driver(0);
	return 0;
}
static struct flashlight_operations ocp8135b_flash_ops = {
	ocp8135b_flash_open,
	ocp8135b_flash_release,
	ocp8135b_ioctl,
	ocp8135b_strobe_store,
	ocp8135b_set_driver
};

static ssize_t ocp8135b_torch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf =  0;
	pr_info("%s : debug:%s", __func__, buf);
	sscanf(buf, "%d", &databuf);

	if ( databuf <= 0 ) { // torch disable
		ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
	} else { //torch enable
		if(ocp8135b_enf_state == OCP8135B_PINCTRL_PINSTATE_HIGH){
			ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
			mdelay(10);
		}
		ocp8135b_enable_ctrl(ocp8135b_flash_data, true);
		mdelay(10);
		ocp8135b_force_torch_duty(databuf);
		ocp8135b_pwm_enable(ocp8135b_flash_data);
	}

	return count;
}
static ssize_t ocp8135b_torch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "%s \n",__func__);
	return len;
}
static ssize_t ocp8135b_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf =  0;
	pr_info("%s : debug:%s", __func__, buf);
	sscanf(buf, "%d", &databuf);

	ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
	if ( databuf > 0 ) {
		ocp8135b_force_flash_duty(databuf - 1);
		ocp8135b_pwm_enable(ocp8135b_flash_data);
		mdelay(10);
		ocp8135b_pinctrl_set(ocp8135b_flash_data, OCP8135B_PINCTRL_PIN_ENF, OCP8135B_PINCTRL_PINSTATE_HIGH);
	}
	mdelay(800);
	ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
	return count;
}
static ssize_t ocp8135b_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "%s \n",__func__);
	return len;
}
static DEVICE_ATTR(OCP8135B_TORCH, 0664, ocp8135b_torch_show, ocp8135b_torch_store);
static DEVICE_ATTR(OCP8135B_FLASH, 0664, ocp8135b_flash_show, ocp8135b_flash_store);
static struct attribute *flashlight_ocp8135b_attrs[ ] = {
	&dev_attr_OCP8135B_TORCH.attr,
	&dev_attr_OCP8135B_FLASH.attr,
	NULL,
};
static int ocp8135b_sysfs_create_files(struct platform_device *pdev)
{
	int err = 0, i = 0;
	for ( i = 0; flashlight_ocp8135b_attrs[i] != NULL; i++) {
		err = sysfs_create_file(&pdev->dev.kobj, flashlight_ocp8135b_attrs[i]);
		if ( err < 0) {
			pr_err("%s: create file node fail err = %d\n", __func__, err);
			return err;
		}
	}
	return 0;
}

static int ocp8135b_parse_dt(struct ocp8135b_flash *flash){
	struct device_node *np, *child;
	struct device *dev = flash->dev;
	const char *fled_name = "flash";
	int  ret = 0;
	if (!dev || !dev->of_node){
		pr_info("[%s] Err! dev or node is null", __func__);
		return -ENODEV;
	}
	np = dev->of_node;
	child = of_get_child_by_name(np, fled_name);
	if(child){
		if (of_property_read_u32(child, "type",
					&flash->flash_dev_id.type))
			goto err_node_put;
		if (of_property_read_u32(child,
					"ct", &flash->flash_dev_id.ct))
			goto err_node_put;
		if (of_property_read_u32(child,
					"part", &flash->flash_dev_id.part))
			goto err_node_put;
		ret = snprintf(flash->flash_dev_id.name,
				FLASHLIGHT_NAME_SIZE,
				flash->subdev_led.name);
		if (ret < 0)
			pr_info("snprintf failed\n");
		flash->flash_dev_id.channel = 0;
		flash->flash_dev_id.decouple = 0;
		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				flash->flash_dev_id.type,
				flash->flash_dev_id.ct,
				flash->flash_dev_id.part,
				flash->flash_dev_id.name,
				flash->flash_dev_id.channel,
				flash->flash_dev_id.decouple);
		if (flashlight_dev_register_by_device_id(&flash->flash_dev_id,
			&ocp8135b_flash_ops))
			return -EFAULT;
	}
	return 0;
err_node_put:
	of_node_put(child);
	return -EINVAL;
}

static void ocp8135b_work_disable(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	ocp8135b_mode_ctrl(ocp8135b_flash_data);
	ocp8135b_enable_ctrl(ocp8135b_flash_data, false);
}

static enum hrtimer_restart ocp8135b_timer_func(struct hrtimer *timer)
{
	schedule_work(&ocp8135b_work);
	return HRTIMER_NORESTART;
}

static int ocp8135b_probe(struct platform_device *pdev)
{
	struct ocp8135b_flash *flash;
	struct ocp8135b_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int rval;
	pr_info("%s Probe start.\n",__func__);
	flash = devm_kzalloc(&pdev->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;
	/* init platform data */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->max_flash_timeout = OCP8135B_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt = OCP8135B_FLASH_BRT_MAX;
		pdata->max_torch_brt = OCP8135B_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &pdev->dev;
	mutex_init(&flash->lock);
	ocp8135b_flash_data = flash;

	INIT_WORK(&ocp8135b_work, ocp8135b_work_disable);
	hrtimer_init(&ocp8135b_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp8135b_timer.function = ocp8135b_timer_func;
	ocp8135b_timeout_ms = 100;

	rval = ocp8135b_pinctrl_init(flash);
	if (rval < 0){
		pr_info("[%s] ocp8135b pinctrl init fail !", __func__);
		return rval;
	}
	rval = ocp8135b_subdev_init(flash, "ocp8135b-led0");
	if (rval < 0){
		pr_info("[%s] ocp8135b subdev init fail !", __func__);
		return rval;
	}
	rval = ocp8135b_parse_dt(flash);
	if (rval < 0){
		pr_info("[%s] ocp8135b parse dt fail !", __func__);
		return rval;
	}
	/* clear usage count */
	use_count = 0;
	ocp8135b_sysfs_create_files(pdev);
	pr_info("%s Probe done.\n",__func__);
	return 0;
}
static int ocp8135b_remove(struct platform_device *pdev)
{
	struct ocp8135b_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct ocp8135b_flash *flash = container_of(&pdata, struct ocp8135b_flash, pdata);
	pr_info("Remove start.\n");
	pdev->dev.platform_data = NULL;
	v4l2_device_unregister_subdev(&flash->subdev_led);
	v4l2_ctrl_handler_free(&flash->ctrls_led);
	media_entity_cleanup(&flash->subdev_led.entity);
	flush_work(&ocp8135b_work);
	/* flush work queue */
	pr_info("Remove done.\n");
	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id ocp8135b_gpio_of_match[] = {
	{.compatible = OCP8135B_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp8135b_gpio_of_match);
#else
static struct platform_device ocp8135b_gpio_platform_device[] = {
	{
		.name = OCP8135B_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ocp8135b_gpio_platform_device);
#endif
static struct platform_driver ocp8135b_platform_driver = {
	.probe = ocp8135b_probe,
	.remove = ocp8135b_remove,
	.driver = {
		.name = OCP8135B_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ocp8135b_gpio_of_match,
#endif
	},
};
static int __init flashlight_ocp8135b_init(void)
{
	int ret;
	pr_info("Init start.\n");
#ifndef CONFIG_OF
	ret = platform_device_register(&ocp8135b_gpio_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif
	ret = platform_driver_register(&ocp8135b_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}
	pr_info("Init done.\n");
	return 0;
}
static void __exit flashlight_ocp8135b_exit(void)
{
	pr_info("Exit start.\n");
	platform_driver_unregister(&ocp8135b_platform_driver);
	pr_info("Exit done.\n");
}
module_init(flashlight_ocp8135b_init);
module_exit(flashlight_ocp8135b_exit);
MODULE_AUTHOR("Roger-HY Wang <roger-hy.wang@mediatek.com>");
MODULE_DESCRIPTION("OCP8135B LED flash driver");
MODULE_LICENSE("GPL");
