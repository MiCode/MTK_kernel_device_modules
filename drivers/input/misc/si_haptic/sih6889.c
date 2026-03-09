/*
 *  Silicon Integrated Co., Ltd haptic sih6889 driver file
 *
 *  Copyright (c) 2024 shanfa <shanfa.tang@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>

#include "sih6889_reg.h"
#include "sih6889.h"
#include "haptic.h"
#include "haptic_mid.h"
#include "haptic_regmap.h"
#include "sih6889_func_config.h"

static void sih6889_unlock(sih_haptic_t *);
static void sih6889_stop(sih_haptic_t *);

// static uint8_t sih6889_iis_addrs[3] = { SIH6889_REG_IIS_CONF0,
// 								 		SIH6889_REG_IIS_CONF1,
// 								 		SIH6889_REG_IIS_CONF2 };

// /* default pvdd values in low power */
// static int s_lp_pvdds[SIH_LP_INDEX] = { SIH6889_LP_PVDD_6_5V, SIH6889_LP_PVDD_7_0V,
// 										SIH6889_LP_PVDD_7_5V, SIH6889_LP_PVDD_9_0V };
// /* default ipeak values in low power */
// static int s_lp_ipeaks[SIH_LP_INDEX] = { SIH6889_LP_IPEAK_A, SIH6889_LP_IPEAK_A,
// 										 SIH6889_LP_IPEAK_A, SIH6889_LP_IPEAK_A };

/***********************************************
*
* chip reg config
*
***********************************************/
static void sih6889_software_reset(sih_haptic_t *sih_haptic)
{
	uint8_t reg_value = SIH6889_ID_SOFTWARE_RESET;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ID, SIH_I2C_OPERA_BYTE_ONE, &reg_value);
	usleep_range(3500, 4000);
}

static void sih6889_hardware_reset(sih_haptic_t *sih_haptic)
{
	uint8_t reg_value = SIH6889_ID_HARDWARE_RESET;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ID, SIH_I2C_OPERA_BYTE_ONE, &reg_value);
	usleep_range(3500, 4000);
}

static int sih6889_probe(sih_haptic_t *sih_haptic)
{
	int ret = -1;
	uint8_t i;
	uint8_t chip_id_value = 0;

	for (i = 0; i < SIH6889_READ_CHIP_ID_MAX_TRY; i++) {
		ret = i2c_read_bytes(sih_haptic, SIH6889_REG_ID,
			&chip_id_value, SIH_I2C_OPERA_BYTE_ONE);
		if (ret < 0) {
			hp_err("%s:i2c read id failed\n", __func__);
		} else {
			if (chip_id_value == SIH6889_CHIPID_REG_VALUE) {
				hp_info("%s: i2c read id success, chip_id = 0x%x\n",
					__func__, chip_id_value);
				sih6889_unlock(sih_haptic);
				return 0;
			}
		}
		usleep_range(2000, 2500);
	}

	return -ENODEV;
}

static void sih6889_ram_init(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSCTRL1_BIT_RAMINIT_EN :
		SIH6889_SYSCTRL1_BIT_RAMINIT_OFF;
	hp_info("%s: enter! flag = %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL1, SIH6889_SYSCTRL1_BIT_ENRAMINIT_MASK, val);
}

static void sih6889_detect_fifo_ctrl(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_DETECT_FIFO_CTRL_EN :
		SIH6889_DETECT_FIFO_CTRL_OFF;
	hp_info("%s: enter! flag = %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL,
		SIH6889_DETECT_FIFO_CTRL_MASK, val);
}

static void sih6889_f0_tracking(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_MODECTRL_BIT_TRACK_F0_EN :
		SIH6889_MODECTRL_BIT_TRACK_F0_OFF;
	hp_info("%s: enter! flag = %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAIN_STATE_CTRL, SIH6889_MODECTRL_BIT_TRACK_F0_MASK, val);
}

static void sih6889_detect_done_int(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSINT2_BIT_F0_DETECT_DONE_INT_EN :
		SIH6889_SYSINT2_BIT_F0_DETECT_DONE_INT_OFF;
	hp_info("%s: enter! flag = %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINTM2, SIH6889_SYSINT2_BIT_F0_DETECT_DONE_INT_MASK, val);
}

static void sih6889_set_boost_mode(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSCTRL2_BIT_BOOST_ENABLE :
		SIH6889_SYSCTRL2_BIT_BOOST_BYPASS;
	hp_info("%s: haptic boost mode = %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL2, SIH6889_SYSCTRL2_BIT_BOOST_BYPASS_MASK, val);
}

static void sih6889_set_go_enable(sih_haptic_t *sih_haptic,
	uint8_t play_mode)
{
	hp_info("%s: enter! go mode = %d\n", __func__, play_mode);
	switch (play_mode) {
	case SIH_RAM_MODE:
	case SIH_RAM_LOOP_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_GO, SIH6889_GO_BIT_RAM_GO_MASK, SIH6889_GO_BIT_RAM_GO_ENABLE);
		break;
	case SIH_RTP_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_GO, SIH6889_GO_BIT_RTP_GO_MASK, SIH6889_GO_BIT_RTP_GO_ENABLE);
		break;
	case SIH_CONT_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_GO, SIH6889_GO_BIT_F0_SEQ_GO_MASK,
			SIH6889_GO_BIT_F0_SEQ_GO_ENABLE);
		break;
	default:
		hp_err("%s: play mode = %d, no need to go\n",  __func__,
			sih_haptic->chip_ipara.play_mode);
		break;
	}
}

static void sih6889_set_go_disable(sih_haptic_t *sih_haptic)
{
    uint8_t reg_val = 0x10;

	hp_info("%s:enter!\n", __func__);
    haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_GO, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih6889_play_go(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter, play_mode = %d, flag = %d\n", __func__,
		sih_haptic->chip_ipara.play_mode, flag);
	if (flag) {
		sih6889_set_go_enable(sih_haptic, sih_haptic->chip_ipara.play_mode);
		sih_haptic->chip_ipara.kpre_time = ktime_get();
	} else {
		sih_haptic->chip_ipara.kcur_time = ktime_get();
		sih_haptic->chip_ipara.interval_us =
			ktime_to_us(ktime_sub(sih_haptic->chip_ipara.kcur_time,
			sih_haptic->chip_ipara.kpre_time));
		if (sih_haptic->chip_ipara.interval_us < 2000) {
			hp_info("%s: sih6889->interval_us = %d < 2000\n",
				__func__, sih_haptic->chip_ipara.interval_us);
			udelay(1000);
		}
		sih6889_set_go_disable(sih_haptic);
	}
}

static void sih6889_clear_interrupt_state(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	hp_info("%s: enter\n", __func__);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih6889_set_gain(sih_haptic_t *sih_haptic, uint8_t gain)
{
	hp_info("%s: set gain 0x%02x\n", __func__, gain);
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_PWM_PRE_GAIN, SIH_I2C_OPERA_BYTE_ONE, &gain);
}

static void sih6889_f0_cali(sih_haptic_t *sih_haptic)
{
	int32_t tmp = 0;
	int32_t code = 0;


	if (sih_haptic->detect.tracking_f0 > SIH6889_F0_VAL_MAX ||
		sih_haptic->detect.tracking_f0 < SIH6889_F0_VAL_MIN)
			code = 0;
	else {
		tmp = (int32_t)(sih_haptic->detect.tracking_f0 * SIH6889_F0_CAL_COE
			/ sih_haptic->detect.cali_target_value);
		code = (tmp - SIH6889_F0_CAL_COE) / SIH6889_F0_CALI_DELTA;
		/*
		* f0 calibration formulation:
		*
		* code = (tracking_f0 / target_f0 - 1) / 0.00288
		*
		* 0.00288 is calc coefficient
		*/
	}
	hp_info("%s:cali data:0x%02x\n", __func__, code);
	sih_haptic->detect.f0_cali_data = (uint8_t)code;
}

static void sih6889_upload_f0(sih_haptic_t *sih_haptic, uint8_t flag)
{
	uint8_t reg_val = 0;

	switch (flag) {
	case SIH_WRITE_ZERO:
		reg_val = 0;
		break;
	case SIH_F0_CALI_LRA:
		sih6889_f0_cali(sih_haptic);
		reg_val = sih_haptic->detect.f0_cali_data;
		break;
	case SIH_OSC_CALI_LRA:
		reg_val = sih_haptic->osc_para.osc_data;
		break;
	default:
		hp_err("%s: err flag\n", __func__);
		return;
	}
	hp_info("%s: trim code = 0x%02x\n", __func__, reg_val);
	haptic_regmap_write(sih_haptic->regmapp.regmapping, SIH6889_REG_TRIM1,
		SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static bool sih6889_if_chip_is_done(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH6889_SYSINT_BIT_DONE) == SIH6889_SYSINT_BIT_DONE;

	return flag;
}

static void sih6889_get_vbat_only(sih_haptic_t *sih_haptic)
{
	uint8_t time = SIH6889_GET_VBAT_MAX_TRY;
	uint8_t vbat_high;
	uint8_t vbat_low;
	uint32_t vbat_raw_data;

	hp_info("%s: enter\n", __func__);
	/* enter standby mode */
	sih6889_stop(sih_haptic);
	/* load detect vbat config */
	sih6889_load_func_config(sih_haptic, REG_FUNC_VBAT);

	while (time--) {
		if (sih6889_if_chip_is_done(sih_haptic)) {
			usleep_range(5000, 5500);
			/* read raw data */
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH6889_REG_ADC_OC_DATA_H, SIH_I2C_OPERA_BYTE_ONE, &vbat_high);
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH6889_REG_ADC_VBAT_DATA_L, SIH_I2C_OPERA_BYTE_ONE, &vbat_low);
			vbat_raw_data = (uint32_t)((vbat_high & 0xf0) << 4 | vbat_low);
			/* calc vbat */
			/*
			* vbat calc formulation:
			* VBAT = raw_data / 2048 * 1.6 * 4;
			* 1.6 is adc vref voltage
			* 4 is lpf amplify coefficient
			* 
			* the calculated value is 100 times the actual value.
			*/
			sih_haptic->detect.vbat = vbat_raw_data *
				SIH6889_ADC_COE / SIH6889_ADC_AMPLIFY_COE;
			hp_info("%s: 0x58 = 0x%02x, 0x59 = 0x%02x\n", __func__, 
				vbat_high, vbat_low);
			break;
		}

		hp_info("%s: wait for detect done int\n", __func__);
		usleep_range(2000, 2500);
	}

	hp_info("detect_vbat = %d\n", sih_haptic->detect.vbat);
}

static void __maybe_unused sih6889_vbat_comp(sih_haptic_t *sih_haptic)
{
	uint32_t comp_gain = 0x80;
	uint32_t curr_vbat = 0;

	hp_info("%s\n", __func__);

	curr_vbat = sih_haptic->detect.vbat;

	if (curr_vbat < SIH6889_VBAT_MIN) {
		curr_vbat = SIH6889_VBAT_MIN;
		hp_info("%s: vbat is lower than min, set to min\n", __func__);
	} else if (curr_vbat > SIH6889_VBAT_MAX) {
		curr_vbat = SIH6889_VBAT_MAX;
		hp_info("%s: vbat is higher than max, set to max\n", __func__);
	}
	/*
	* vbat compensation formulation:
	*
	* comp_gain * curreng_vbat = current_gain * standard_vbat
	*/
	if (0 == sih_haptic->ram.loop_voltage) {
		sih_haptic->ram.loop_voltage = SIH6889_STANDARD_VBAT;
	}
	comp_gain = sih_haptic->ram.loop_voltage * sih_haptic->chip_ipara.gain / curr_vbat;
	if (comp_gain > SIH_HAPTIC_MAX_GAIN) {
		comp_gain = SIH_HAPTIC_MAX_GAIN;
		hp_info("%s: comp_gain is higher than max gain, set to max\n", __func__);
	}
	sih6889_set_gain(sih_haptic, comp_gain);
}

static void sih6889_set_play_mode(sih_haptic_t *sih_haptic,
	uint8_t play_mode)
{
	hp_info("%s:enter! play mode = %d\n", __func__, play_mode);

	/* auto set pvdd by detect_vbat */
	// if (play_mode != SIH_IDLE_MODE) {
	// 	sih6889_get_vbat(sih_haptic);
	// }

	switch (play_mode) {
	case SIH_IDLE_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
		sih6889_set_boost_mode(sih_haptic, true);
		hp_info("%s:now chip is stanby\n", __func__);
		break;
	case SIH_RAM_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_MODE;
		sih6889_upload_f0(sih_haptic, SIH_F0_CALI_LRA);
		break;
	case SIH_RTP_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RTP_MODE;
		sih6889_upload_f0(sih_haptic, SIH_F0_CALI_LRA);
		break;
	case SIH_TRIG_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_TRIG_MODE;
		break;
	case SIH_CONT_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_CONT_MODE;
		break;
	case SIH_RAM_LOOP_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_LOOP_MODE;
		sih6889_upload_f0(sih_haptic, SIH_F0_CALI_LRA);
		sih6889_set_boost_mode(sih_haptic, false);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_WAVELOOP1, SIH6889_WAVELOOP1_BIT_SEQ1_MASK,
			SIH6889_WAVELOOP1_BIT_SEQ1_INFINITE);
		break;
	default:
		hp_err("%s: play mode err, mode = %d\n", __func__, play_mode);
		break;
	}
	sih6889_clear_interrupt_state(sih_haptic);
}

static void sih6889_set_bst_ea(sih_haptic_t *sih_haptic, uint32_t drv_bst, uint8_t mode)
{
	uint8_t reg_addr, mask, shift;

	switch (mode) {
	case SIH6889_BST_MODE_PLAYBACK:
		reg_addr = SIH6889_REG_ANA_CTRL2;
		mask = SIH6889_ANA_CTRL2_BST_EA_SEL_O_MASK;
		shift = 0;
		break;
	case SIH6889_BST_MODE_BRK:
		reg_addr = SIH6889_REG_BRK_ANA1;
		mask = SIH6889_BRK_ANA1_BIT_BST_BRK_EA_SEL_O_MASK;
		shift = 4;
		break;
	case SIH6889_BST_MODE_TRIG:
		reg_addr = SIH6889_REG_TRIG_ANA1;
		mask = SIH6889_TRIG_ANA1_BIT_BST_TRIG_EA_SEL_O_MASK;
		shift = 4;
		break;
	default:
		return;
	}

	if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_6 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_7)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EA_6_7 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_7 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_9)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EA_7_9 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_9 &&
		drv_bst <= SIH6889_ANA_CTRL_BST_LEVEL_12)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EA_9_12 << shift);
	else
		hp_err("%s: vboost %d out of range!\n", __func__, drv_bst);
}

static void sih6889_set_bst_eavrf(sih_haptic_t *sih_haptic, uint32_t drv_bst, uint8_t mode)
{
	uint8_t reg_addr, mask, shift;

	switch (mode) {
	case SIH6889_BST_MODE_PLAYBACK:
		reg_addr = SIH6889_REG_ANA_CTRL5;
		mask = SIH6889_ANA_CTRL5_BST_EAVRF_SEL_O_MASK;
		shift = 3;
		break;
	case SIH6889_BST_MODE_BRK:
		reg_addr = SIH6889_REG_BRK_ANA0;
		mask = SIH6889_BRK_ANA0_BIT_BST_BRK_EAVRF_SEL_O_MASK;
		shift = 2;
		break;
	case SIH6889_BST_MODE_TRIG:
		reg_addr = SIH6889_REG_TRIG_ANA0;
		mask = SIH6889_TRIG_ANA0_BIT_BST_TRIG_EAVRF_SEL_O_MASK;
		shift = 2;
		break;
	default:
		return;
	}

	if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_6 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_7)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_6_7 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_7 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_8)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_7_8 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_8 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_9)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_8_9 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_9 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_10)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_9_10 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_10 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_11)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_10_11 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_11 &&
		drv_bst <= SIH6889_ANA_CTRL_BST_LEVEL_12)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_EAVRF_SEL_11_12 << shift);
	else
		hp_err("%s: vboost %d out of range!\n", __func__, drv_bst);
}

static void sih6889_set_bst_zcd(sih_haptic_t *sih_haptic, uint32_t drv_bst, uint8_t mode)
{
	uint8_t reg_addr, mask, shift;

	switch (mode) {
	case SIH6889_BST_MODE_PLAYBACK:
		reg_addr = SIH6889_REG_ANA_CTRL6;
		mask = SIH6889_ANA_CTRL6_BST_ZCD_IOS_O_MASK;
		shift = 4;
		break;
	case SIH6889_BST_MODE_BRK:
		reg_addr = SIH6889_REG_BRK_ANA0;
		mask = SIH6889_BRK_ANA0_BIT_BST_BRK_ZCD_IOS_O_MASK;
		shift = 0;
		break;
	case SIH6889_BST_MODE_TRIG:
		reg_addr = SIH6889_REG_TRIG_ANA0;
		mask = SIH6889_TRIG_ANA0_BIT_BST_TRIG_ZCD_IOS_O_MASK;
		shift = 0;
		break;
	default:
		return;
	}

	if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_6 &&
		drv_bst < SIH6889_ANA_CTRL_BST_LEVEL_8)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_ZCD_IOS_6_8 << shift);
	else if (drv_bst >= SIH6889_ANA_CTRL_BST_LEVEL_8 &&
		drv_bst <= SIH6889_ANA_CTRL_BST_LEVEL_12)
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			reg_addr, mask, SIH6889_ANA_BST_ZCD_IOS_8_12 << shift);
	else
		hp_err("%s: vboost %d out of range!\n", __func__, drv_bst);
}

static void sih6889_set_playback_bst_vol(sih_haptic_t *sih_haptic,
	uint32_t drv_bst)
{
	uint8_t bst_reg_val = 0;

	/*
	* drv boost calc formulation:
	* reg_val = drv_bst - 4.5 / 0.0625;
	*
	* 4.5 is base voltage, 0.00625 is step
	*/
	bst_reg_val = (uint8_t)(((drv_bst - SIH6889_DRV_BOOST_BASE) *
		SIH6889_DRV_BOOST_SETP_COE) / SIH6889_DRV_BOOST_SETP);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ANA_CTRL3, SIH6889_ANA_CTRL3_BST_OUT_SEL_O_MASK, bst_reg_val);

	sih6889_set_bst_ea(sih_haptic, drv_bst, SIH6889_BST_MODE_PLAYBACK);
	sih6889_set_bst_eavrf(sih_haptic, drv_bst, SIH6889_BST_MODE_PLAYBACK);
	sih6889_set_bst_zcd(sih_haptic, drv_bst, SIH6889_BST_MODE_PLAYBACK);
}

static void sih6889_set_brk_bst_vol(sih_haptic_t *sih_haptic, uint32_t drv_bst)
{
	uint8_t bst_reg_val = 0;

	/*
	* drv boost calc formulation:
	* reg_val = drv_bst - 4.5 / 0.0625;
	*
	* 4.5 is base voltage, 0.00625 is step
	*/
	bst_reg_val = (uint8_t)(((drv_bst - SIH6889_DRV_BOOST_BASE) *
		SIH6889_DRV_BOOST_SETP_COE) / SIH6889_DRV_BOOST_SETP);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_BRK_BOOST, SIH6889_BRK_BOOST_BIT_BRK_BOOST_MASK, bst_reg_val);

	sih6889_set_bst_ea(sih_haptic, drv_bst, SIH6889_BST_MODE_BRK);
	sih6889_set_bst_eavrf(sih_haptic, drv_bst, SIH6889_BST_MODE_BRK);
	sih6889_set_bst_zcd(sih_haptic, drv_bst, SIH6889_BST_MODE_BRK);
}

static void sih6889_set_trig_bst_vol(sih_haptic_t *sih_haptic, uint32_t drv_bst)
{
	uint8_t bst_reg_val = 0;

	/*
	* drv boost calc formulation:
	* reg_val = drv_bst - 4.5 / 0.0625;
	*
	* 4.5 is base voltage, 0.00625 is step
	*/
	bst_reg_val = (uint8_t)(((drv_bst - SIH6889_DRV_BOOST_BASE) *
		SIH6889_DRV_BOOST_SETP_COE) / SIH6889_DRV_BOOST_SETP);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG_BOOST, SIH6889_TRIG_BOOST_BIT_TRIG_BOOST_MASK, bst_reg_val);

	sih6889_set_bst_ea(sih_haptic, drv_bst, SIH6889_BST_MODE_TRIG);
	sih6889_set_bst_eavrf(sih_haptic, drv_bst, SIH6889_BST_MODE_TRIG);
	sih6889_set_bst_zcd(sih_haptic, drv_bst, SIH6889_BST_MODE_TRIG);
}

static void sih6889_set_brk_bst(sih_haptic_t *sih_haptic, uint32_t bst_vol)
{
	uint32_t tmp = 0;
	uint8_t write_value = 0;

	tmp = bst_vol * SIH6889_VBOOST_MUL_COE / SIH6889_BRK_VBOOST_COE;
	write_value = (uint8_t)((tmp >> 8) & 0xff);

	/* set trig vboost value */
	sih6889_set_brk_bst_vol(sih_haptic, bst_vol);
	/* set the max voltage actually allowed */
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_V_BOOST, SIH_I2C_OPERA_BYTE_ONE, &write_value);
}

static void sih6889_set_drv_bst_vol(sih_haptic_t *sih_haptic, uint32_t drv_bst)
{
	sih6889_set_playback_bst_vol(sih_haptic, drv_bst);
}

static void sih6889_set_auto_pvdd(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSCTRL1_BIT_AUTO_PVDD_EN :
		SIH6889_SYSCTRL1_BIT_AUTO_PVDD_OFF;
	sih_haptic->chip_ipara.auto_pvdd_en = flag;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL1, SIH6889_SYSCTRL1_BIT_AUTO_PVDD_MASK, val);
}

static void sih6889_interrupt_state_init(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;
	uint8_t reg_mask = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	hp_info("%s:int state 0x02 = 0x%02x\n", __func__, reg_val);

	/* int enable */
	reg_mask = SIH6889_SYSINT_BIT_OCP_FLAG_INT_MASK | SIH6889_SYSINTM_BIT_DONE_MASK |
		SIH6889_SYSINT_BIT_UVP_FLAG_INT_MASK | SIH6889_SYSINT_BIT_OTP_FLAG_INT_MASK |
		SIH6889_SYSINT_BIT_MODE_SWITCH_INT_MASK | SIH6889_SYSINTM_BIT_FF_AEI_MASK |
		SIH6889_SYSINT_BIT_BRK_LONG_TIMEOUT_MASK | SIH6889_SYSINTM_BIT_FF_AFI_MASK;
	reg_val = SIH6889_SYSINT_BIT_OCP_FLAG_INT_OFF | SIH6889_SYSINTM_BIT_DONE_OFF |
		SIH6889_SYSINT_BIT_UVP_FLAG_INT_OFF | SIH6889_SYSINT_BIT_OTP_FLAG_INT_OFF |
		SIH6889_SYSINT_BIT_MODE_SWITCH_INT_OFF | SIH6889_SYSINTM_BIT_FF_AEI_OFF |
		SIH6889_SYSINT_BIT_BRK_LONG_TIMEOUT_OFF | SIH6889_SYSINTM_BIT_FF_AFI_OFF;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINTM, reg_mask, reg_val);
}

static void sih6889_set_low_power_mode(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSCTRL1_BIT_LOWPOWER_EN :
		SIH6889_SYSCTRL1_BIT_LOWPOWER_OFF;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL1, SIH6889_SYSCTRL1_BIT_LOWPOWER_MASK, val);
	sih_haptic->chip_ipara.low_power = flag;

	hp_info("%s: sih_haptic->chip_ipara.low_power = %d\n",
		__func__, sih_haptic->chip_ipara.low_power);
}

static void sih6889_set_brk_state(sih_haptic_t *sih_haptic,
	uint8_t mode, bool flag)
{
	uint8_t brk_en;
	uint8_t brk_mask;

	switch (mode) {
	case SIH_RAM_MODE:
		sih_haptic->brake_para.ram_brake_en = flag;
		brk_en = flag ? SIH6889_MODECTRL_BIT_RAM_BRK_EN :
			SIH6889_MODECTRL_BIT_RAM_BRK_OFF;
		brk_mask = SIH6889_MODECTRL_BIT_RAM_BRK_MASK;
		break;
	case SIH_RTP_MODE:
		sih_haptic->brake_para.rtp_brake_en = flag;
		brk_en = flag ? SIH6889_MODECTRL_BIT_RTP_BRK_EN :
			SIH6889_MODECTRL_BIT_RTP_BRK_OFF;
		brk_mask = SIH6889_MODECTRL_BIT_RTP_BRK_MASK;
		break;
	case SIH_TRIG_MODE:
		sih_haptic->brake_para.trig_brake_en = flag;
		brk_en = flag ? SIH6889_MODECTRL_BIT_TRIG_BRK_EN :
			SIH6889_MODECTRL_BIT_TRIG_BRK_OFF;
		brk_mask = SIH6889_MODECTRL_BIT_TRIG_BRK_MASK;
		break;
	case SIH_CONT_MODE:
		sih_haptic->brake_para.cont_brake_en = flag;
		brk_en = flag ? SIH6889_MODECTRL_BIT_TRACK_BRK_EN :
			SIH6889_MODECTRL_BIT_TRACK_BRK_OFF;
		brk_mask = SIH6889_MODECTRL_BIT_TRACK_BRK_MASK;
		break;
	default:
		hp_err("%s: play mode err, mode = %d\n", __func__, mode);
		return;
	}
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAIN_STATE_CTRL, brk_mask, brk_en);
}

static size_t sih6889_get_brk_state(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t reg_val = 0;
	size_t len = 0;

	hp_info("%s: enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAIN_STATE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "brake_state = %d\n", reg_val);

	return len;
}

static void sih6889_set_pwm_rate(sih_haptic_t *sih_haptic,
	uint8_t sample_rpt, uint8_t sample_en)
{
	switch (sample_rpt) {
	case SIH_SAMPLE_RPT_ONE_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_PWM_UP_SAMPLE_CTRL,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_ONE_TIME);
		break;
	case SIH_SAMPLE_RPT_TWO_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_PWM_UP_SAMPLE_CTRL,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_TWO_TIME);
		break;
	case SIH_SAMPLE_RPT_FOUR_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_PWM_UP_SAMPLE_CTRL,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_FOUR_TIME);
		break;
	default:
		hp_err("%s: pwm_state sample rate err, sample_rpt = %d\n",
			__func__, sample_rpt);
		break;
	}

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_PWM_UP_SAMPLE_CTRL,
		SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_MASK,
		sample_en ? SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_EN :
		SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_OFF);
}

static size_t sih6889_get_pwm_rate(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t pwm_state = 0;
	size_t len = 0;

	hp_info("%s: enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_PWM_UP_SAMPLE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &pwm_state);

	len += snprintf(buf + len, PAGE_SIZE - len, "pwm_state = %d\n", pwm_state);

	return len;
}

/***********************************************
*
* chip state check
*
***********************************************/

static bool sih6889_if_chip_is_standby(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH6889_SYSSST_BIT_STANDBY) == SIH6889_SYSSST_BIT_STANDBY;

	hp_info("%s: reg_val = 0x%02x\n", __func__, reg_val);
	return flag;
}

static bool sih6889_if_chip_is_detect_done(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT2, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH6889_SYSINT2_BIT_F0_DETECT_DONE_INT) ==
		SIH6889_SYSINT2_BIT_F0_DETECT_DONE_INT;

	return flag;
}

static bool sih6889_if_chip_is_mode(sih_haptic_t *sih_haptic, uint8_t mode)
{
	uint8_t reg_val = 0;
	bool flag = false;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	switch (mode) {
	case SIH_IDLE_MODE:
		flag = (reg_val & SIH6889_SYSSST_BIT_STANDBY) == SIH6889_SYSSST_BIT_STANDBY;
		break;
	case SIH_RAM_MODE:
	case SIH_RAM_LOOP_MODE:
		flag = (reg_val & SIH6889_SYSSST_BIT_RAM_STATE) == SIH6889_SYSSST_BIT_RAM_STATE;
		break;
	case SIH_RTP_MODE:
		flag = (reg_val & SIH6889_SYSSST_BIT_RTP_STATE) == SIH6889_SYSSST_BIT_RTP_STATE;
		break;
	case SIH_TRIG_MODE:
		flag = (reg_val & SIH6889_SYSSST_BIT_TRIG_STATE) ==
			SIH6889_SYSSST_BIT_TRIG_STATE;
		break;
	case SIH_CONT_MODE:
		flag = (reg_val & SIH6889_SYSSST_BIT_F0_TRACK_STATE) ==
			SIH6889_SYSSST_BIT_F0_TRACK_STATE;
		break;
	default:
		hp_err("%s: err mode!\n", __func__);
		break;
	}

	return flag;
}

static bool sih6889_get_rtp_fifo_full_state(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH6889_SYSSST_BIT_FIF0_AF) == SIH6889_SYSSST_BIT_FIF0_AF;

	return flag;
}

static bool sih6889_get_rtp_fifo_empty_state(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH6889_SYSINT_BIT_FF_AEI) == SIH6889_SYSINT_BIT_FF_AEI;

	return flag;
}

/***********************************************
*
* chip function config
*
***********************************************/
static void sih6889_stop(sih_haptic_t *sih_haptic)
{
	uint32_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;

	hp_info("%s:enter\n", __func__);
	/* wait for last short vibration over */
	while (1) {
		if (!sih6889_if_chip_is_standby(sih_haptic)) {
			sih_haptic->chip_ipara.kcur_time = ktime_get();
			sih_haptic->chip_ipara.interval_us =
				ktime_to_us(ktime_sub(sih_haptic->chip_ipara.kcur_time,
				sih_haptic->chip_ipara.kpre_time));
			if (sih_haptic->chip_ipara.interval_us > SIH_PROTECTION_TIME)
				break;
			hp_info("%s: play time us = %d, less than 30ms, wait\n",
				__func__, sih_haptic->chip_ipara.interval_us);
			udelay(2000);
		} else {
			break;
		}
	}
#if 0
	/* close grab_play_en */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping, SIH6889_REG_SYSCTRL1,
		SIH6889_SYSCTRL1_BIT_GRAB_PLAY_EN_MASK, SIH6889_SYSCTRL1_BIT_GRAB_PLAY_OFF);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping, SIH6889_REG_SYSCTRL1,
		SIH6889_SYSCTRL1_BIT_NO_GRAB_TRIG_ACK_MASK, SIH6889_SYSCTRL1_BIT_NO_GRAB_TRIG_ACK_EN);
	/* open stop_wait_en */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping, SIH6889_REG_BRK_CTRL1,
		SIH6889_BRK_CTRL1_BIT_STOP_WAIT_EN_MASK, SIH6889_BRK_CTRL1_BIT_STOP_WAIT_EN_ON);
#endif
	/* stop current vibration */
	sih6889_play_go(sih_haptic, false);
	/* wait for auto brake over */
	while (cnt--) {
		if (!sih6889_if_chip_is_standby(sih_haptic))
			hp_info("%s: wait for standby\n", __func__);
		else
			break;
		udelay(2000);
	}
	/* stop chip */
	sih6889_set_play_mode(sih_haptic, SIH_IDLE_MODE);
#if 0
	/* close stop_wait_en */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping, SIH6889_REG_BRK_CTRL1,
		SIH6889_BRK_CTRL1_BIT_STOP_WAIT_EN_MASK, SIH6889_BRK_CTRL1_BIT_STOP_WAIT_EN_OFF);
	/* open grab_play_en */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping, SIH6889_REG_SYSCTRL1,
		SIH6889_SYSCTRL1_BIT_GRAB_PLAY_EN_MASK, SIH6889_SYSCTRL1_BIT_GRAB_PLAY_EN);
#endif
}

static void sih6889_update_chip_state(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	if ((reg_val & SIH6889_SYSSST_BIT_STANDBY) == SIH6889_SYSSST_BIT_STANDBY)
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	else
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;

	switch (reg_val & SIH6889_SYSSST_BIT_CENTRAL_STATE_MASK) {
	case SIH6889_SYSSST_BIT_RAM_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_MODE;
		break;
	case SIH6889_SYSSST_BIT_RTP_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_RTP_MODE;
		break;
	case SIH6889_SYSSST_BIT_TRIG_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_TRIG_MODE;
		break;
	case SIH6889_SYSSST_BIT_F0_TRACK_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_CONT_MODE;
		break;
	default:
		sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
		break;
	}
}

static int sih6889_efuse_check(sih_haptic_t *sih_haptic)
{
	uint8_t w_val = 2;
	uint8_t efuse_data[8] = {0};
	uint8_t crc4_result = 0;
	uint8_t crc4_value = 0;
	int ret = 0;
	int i;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_EFUSE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &w_val);
	/* read efuse0 */
	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_EFUSE_RDATA0, SIH_I2C_OPERA_BYTE_FOUR, efuse_data);
	/* read efuse1 */
	w_val = 6; /* 3b'110 */
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_EFUSE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &w_val);
	// haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
	// 	SIH6889_REG_EFUSE_RDATA0, SIH_I2C_OPERA_BYTE_FOUR, &efuse_data[4]);
	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_EFUSE_RDATA0, SIH_I2C_OPERA_BYTE_TWO, &efuse_data[4]);
	crc4_value = (efuse_data[5] & 0x1f) >> 1;

	/* todo: crc check */
	for (i = 0; i < 8; i++)
		hp_info("\tefuse_data[%d] = 0x%02x\n", i, efuse_data[i]);
	
	efuse_data[3] &= 0x3f;
	efuse_data[5] &= 0x1;
	crc4_result = crc4_itu(efuse_data, ARRAY_LEN(efuse_data));
	if (crc4_result != crc4_value) {
		hp_err("%s: crc4 check failed, crc4_result = 0x%02x, crc4_value = 0x%02x\n",
			__func__, crc4_result, crc4_value);
		return ret;
		//ret = -1;
	}

	hp_info("%s: crc_result = 0x%02x, crc_write = 0x%02x\n", __func__,
		crc4_result, crc4_value);
	return ret;
}

/***********************************************
*
* chip ram config
*
***********************************************/
static int sih6889_check_ram_data(sih_haptic_t *sih_haptic,
	uint8_t *cont_data, uint8_t *ram_data, uint32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			hp_err("%s:check err,addr=0x%02x,ram=0x%02x,file=0x%02x\n",
				__func__, i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
		return 0;
	}
	return 0;
}

static int sih6889_update_ram_config(sih_haptic_t *sih_haptic,
	haptic_container_t *sih_cont)
{
	uint8_t fifo_addr[4] = {0};
	uint32_t shift = 0;
	int i = 0;
	int len = 0;
	int ret = -1;
	char *ram_data = NULL;
	uint8_t test_val;

	hp_info("%s: enter\n", __func__);

	mutex_lock(&sih_haptic->lock);

	sih_haptic->ram.baseaddr_shift = 2;
	sih_haptic->ram.ram_shift = 4;
	/* RAMINIT Enable */
	sih6889_ram_init(sih_haptic, true);
	/* base addr */
	shift = sih_haptic->ram.baseaddr_shift;
	sih_haptic->ram.base_addr = (uint32_t)(sih_cont->data[0 + shift] << 8) |
		(sih_cont->data[1 + shift]);
	fifo_addr[0] = (uint8_t)SIH6889_FIFO_AF_ADDR_L(sih_haptic->ram.base_addr);
	fifo_addr[1] = (uint8_t)SIH6889_FIFO_AE_ADDR_L(sih_haptic->ram.base_addr);
	fifo_addr[2] = (uint8_t)SIH6889_FIFO_AF_ADDR_H(sih_haptic->ram.base_addr);
	fifo_addr[3] = (uint8_t)SIH6889_FIFO_AE_ADDR_H(sih_haptic->ram.base_addr);

	hp_info("%s: base_addr = 0x%04x\n", __func__, sih_haptic->ram.base_addr);
	hp_info("%s: fifo[0] = %d, fifo[1] = %d\n", __func__, fifo_addr[0], fifo_addr[1]);
	hp_info("%s: fifo[2] = %d, fifo[3] = %d\n", __func__, fifo_addr[2], fifo_addr[3]);

	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_BASE_ADDRH, SIH_I2C_OPERA_BYTE_TWO, &sih_cont->data[shift]);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTPCFG1, SIH_I2C_OPERA_BYTE_TWO, fifo_addr);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTPCFG3, SIH6889_RTPCFG3_BIT_FIFO_AFH_MASK, (fifo_addr[2]<<4));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTPCFG3, SIH6889_RTPCFG3_BIT_FIFO_AEH_MASK, fifo_addr[3]);

	/* ram */
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RAMADDRH, SIH_I2C_OPERA_BYTE_TWO, &sih_cont->data[shift]);

	i = sih_haptic->ram.ram_shift;

	if (sih_cont->len > SIH_RAMDATA_BUFFER_SIZE)
		sih_cont->len = SIH_RAMDATA_BUFFER_SIZE;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL, SIH_I2C_OPERA_BYTE_ONE, &test_val);
	if (test_val & SIH6889_WAIT_FIFO_DETECT_MASK) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL, 
			SIH6889_WAIT_FIFO_DETECT_MASK, SIH6889_WAIT_FIFO_DETECT_OFF);
	}
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL, SIH_I2C_OPERA_BYTE_ONE, &test_val);
	hp_info("%s: 0x%02x = 0x%02x\n", __func__, 
		SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL, test_val);

	while (i < sih_cont->len) {
		if ((sih_cont->len - i) <= SIH_RAMDATA_READ_SIZE)
			len = sih_cont->len - i;
		else
			len = SIH_RAMDATA_READ_SIZE;

		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			SIH6889_REG_RAMDATA, len, &sih_cont->data[i]);

		i += len;
	}

	sih6889_ram_init(sih_haptic, false);
	sih6889_ram_init(sih_haptic, true);

	i = sih_haptic->ram.ram_shift;
	ram_data = vmalloc(SIH_RAMDATA_BUFFER_SIZE);
	if (!ram_data)
		hp_err("%s: ram_data vmalloc failed\n", __func__);
	else {
		while (i < sih_cont->len) {
			if ((sih_cont->len - i) <= SIH_RAMDATA_READ_SIZE)
				len = sih_cont->len - i;
			else
				len = SIH_RAMDATA_READ_SIZE;

			haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
				SIH6889_REG_RAMDATA, len, ram_data);
			ret = sih6889_check_ram_data(sih_haptic, &sih_cont->data[i],
				ram_data, len);
			if (ret < 0)
				break;
			i += len;
		}
		if (ret)
			hp_err("%s: ram data check error\n", __func__);
		else
			hp_info("%s: ram data check pass\n", __func__);

		vfree(ram_data);
	}

	/* RAMINIT Disable */
	sih6889_ram_init(sih_haptic, false);
	mutex_unlock(&sih_haptic->lock);

	return ret;
}

static void sih6889_set_wav_seq(sih_haptic_t *sih_haptic,
	uint8_t seq, uint8_t wave)
{
	hp_info("%s: seq = %d, wave = %d\n", __func__, seq, wave);
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_WAVESEQ1 + seq, SIH_I2C_OPERA_BYTE_ONE, &wave);
}

static void sih6889_get_wav_seq(sih_haptic_t *sih_haptic, uint32_t len)
{
	uint8_t i;
	uint8_t reg_val[SIH_HAPTIC_SEQUENCER_SIZE] = {0};

	if (len > SIH_HAPTIC_SEQUENCER_SIZE)
		len = SIH_HAPTIC_SEQUENCER_SIZE;

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_WAVESEQ1, len, reg_val);
	for (i = 0; i < len; i++)
		sih_haptic->ram.seq[i] = reg_val[i];
}


static ssize_t sih6889_get_ram_data(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t *ram_data;
	int i = 0;
	int size = 0;
	ssize_t len = 0;

	ram_data = vmalloc(SIH_RAMDATA_BUFFER_SIZE);
	if (!ram_data)
		return len;

	if (sih_haptic->ram.len < SIH_RAMDATA_BUFFER_SIZE)
		size = sih_haptic->ram.len;
	else
		size = SIH_RAMDATA_BUFFER_SIZE;

	while (i < size) {
		if ((size - i) <= SIH_RAMDATA_READ_SIZE)
			len = size - i;
		else
			len = SIH_RAMDATA_READ_SIZE;

		haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
			SIH6889_REG_RAMDATA, len, &ram_data[i]);

		i += len;
	}

	for (i = 1; i < size; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,", ram_data[i]);

	vfree(ram_data);

	return len;
}

static void sih6889_get_first_wave_addr(sih_haptic_t *sih_haptic, uint8_t *wave_addr)
{
	uint8_t reg_array[3] = {0, 0, 0};

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RAMDATA, SIH_I2C_OPERA_BYTE_THREE, reg_array);

	wave_addr[0] = reg_array[1];
	wave_addr[1] = reg_array[2];

	hp_info("%s: wave_addr[0] = 0x%02x, wave_addr[1] = 0x%02x\n",
		__func__, wave_addr[0], wave_addr[1]);
}

static void sih6889_set_ram_addr(sih_haptic_t *sih_haptic)
{
	uint8_t ram_addr[2] = {0};

	ram_addr[0] = (uint8_t)SIH_RAM_ADDR_H(sih_haptic->ram.base_addr);
	ram_addr[1] = (uint8_t)SIH_RAM_ADDR_L(sih_haptic->ram.base_addr);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_BASE_ADDRH, SIH_I2C_OPERA_BYTE_TWO, ram_addr);
}

static void sih6889_set_wav_loop(sih_haptic_t *sih_haptic,
	uint8_t seq, uint8_t loop)
{
	uint8_t offset;

	hp_info("%s: seq = 0x%02x, loop = 0x%02x\n", __func__, seq, loop);
	offset = ((seq + 1) % 2) * 4;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_WAVELOOP1 + (seq / 2), WAVELOOP_SEQ_EVEN_MASK << offset,
		loop << offset);
}

static void sih6889_get_wav_loop(sih_haptic_t *sih_haptic)
{
	uint8_t i;
	uint8_t reg_val[4] = {0};

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_WAVELOOP1, SIH_I2C_OPERA_BYTE_FOUR, reg_val);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_SIZE / 2; i++) {
		sih_haptic->ram.loop[i * 2 + 0] =
			(reg_val[i] >> 4) & WAVELOOP_SEQ_EVEN_MASK;
		sih_haptic->ram.loop[i * 2 + 1] =
			(reg_val[i] >> 0) & WAVELOOP_SEQ_EVEN_MASK;
	}
}

static void sih6889_set_wav_main_loop(sih_haptic_t *sih_haptic, uint8_t loop)
{
	hp_info("%s: main loop = %d\n", __func__, loop);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAINLOOP, SIH6889_MAINLOOP_BIT_MAIN_LOOP_MASK, loop);
}

static void sih6889_get_wav_main_loop(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAINLOOP, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	sih_haptic->ram.main_loop = reg_val;
}

static void sih6889_set_repeat_seq(sih_haptic_t *sih_haptic, uint8_t seq)
{
	uint8_t first_wave_index = 0;

	sih6889_set_wav_seq(sih_haptic, first_wave_index, seq);
	sih6889_set_wav_loop(sih_haptic, first_wave_index,
		WAVELOOP_SEQ_ODD_INFINNTE_TIME);
}

static void sih6889_set_ram_seq_gain(sih_haptic_t *sih_haptic,
	uint8_t wav, uint8_t gain)
{
	uint8_t offset = (wav % 2) * 4;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_GAIN_SET_SEQ1_0 + (wav / 2),
		WAVEGAIN_SEQ_EVEN_MASK << offset, gain << offset);
}

static size_t sih6889_get_ram_seq_gain(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t i;
	uint8_t reg_val[4] = {0};
	size_t count = 0;

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_GAIN_SET_SEQ1_0, SIH_I2C_OPERA_BYTE_FOUR, reg_val);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_GAIN_SIZE; i++) {
		sih_haptic->ram.gain[i * 2 + 0] = (reg_val[i] >> 0) & 0x0F;
		sih_haptic->ram.gain[i * 2 + 1] = (reg_val[i] >> 4) & 0x0F;
		count += snprintf(buf + count, PAGE_SIZE - count,
			"seq%d gain = 0x%02x\n", i * 2 + 0,
			sih_haptic->ram.gain[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count,
			"seq%d gain = 0x%02x\n", i * 2 + 1,
			sih_haptic->ram.gain[i * 2 + 1]);
	}
	return count;
}

/***********************************************
*
* chip rtp config
*
***********************************************/
static size_t sih6889_write_rtp_data(sih_haptic_t *sih_haptic, uint8_t *data, uint32_t len)
{
	int ret = -1;

	// if (sih_haptic->rtp.rtp_brd_on)
	// 	sih_haptic->i2c->addr = sih_haptic->rtp.brd_slv_addr;
	// else
	// 	sih_haptic->i2c->addr = sih_haptic->chip_attr.default_dev_addr;

	ret = haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTPDATA, len, data);

	// /* recovery i2c device addr to normal address */
	// sih_haptic->i2c->addr = sih_haptic->chip_attr.default_dev_addr;
	
	return ret;
}

static void sih6889_set_rtp_aei(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSINTM_BIT_FF_AEI_EN :
		SIH6889_SYSINTM_BIT_FF_AEI_OFF;
	hp_info("%s: %d\n", __func__, flag);

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINTM, SIH6889_SYSINTM_BIT_FF_AEI_MASK, val);
}

static void sih6889_start_thres(sih_haptic_t *sih_haptic)
{
	hp_info("%s: rtp start thres = %d", __func__,
		sih_haptic->rtp.rtp_start_thres);

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTP_START_THRES, SIH_I2C_OPERA_BYTE_ONE,
		&sih_haptic->rtp.rtp_start_thres);
}

/***********************************************
*
* chip cont config
*
***********************************************/
static void sih6889_set_cont_para(sih_haptic_t *sih_haptic, uint8_t flag,
	uint8_t *data)
{
	uint8_t reg_addr_start = SIH6889_REG_SEQ0_T_DRIVER;

	switch (flag) {
	case SIH6889_CONT_PARA_SEQ0:
		reg_addr_start = SIH6889_REG_SEQ0_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_SEQ1:
		reg_addr_start = SIH6889_REG_SEQ1_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_SEQ2:
		reg_addr_start = SIH6889_REG_SEQ2_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_ASMOOTH:
		/* 6889 no ASMOOTH reg */
		return;
	case SIH6889_CONT_PARA_TH_LEN:
		reg_addr_start = SIH6889_REG_T_0;
		break;
	case SIH6889_CONT_PARA_TH_NUM:
		reg_addr_start = SIH6889_REG_CYCLE0;
		break;
	case SIH6889_CONT_PARA_AMPLI:
		reg_addr_start = SIH6889_REG_VREF0;
		break;
	default:
		hp_err("%s: err flag %d\n", __func__, flag);
		break;
	}
	hp_info("%s: reg_start = 0x%02x, data[0] = 0x%02x, data[1] = 0x%02x, data[2] = 0x%02x\n",
		__func__, reg_addr_start, data[0], data[1], data[2]);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping, reg_addr_start,
		SIH_I2C_OPERA_BYTE_THREE, data);
}

static ssize_t sih6889_get_cont_para(sih_haptic_t *sih_haptic,
	uint8_t flag, char *buf)
{
	ssize_t len = 0;
	uint8_t reg_addr_start = SIH6889_REG_SEQ0_T_DRIVER;
	uint8_t reg_val[3] = {0};

	switch (flag) {
	case SIH6889_CONT_PARA_SEQ0:
		reg_addr_start = SIH6889_REG_SEQ0_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_SEQ1:
		reg_addr_start = SIH6889_REG_SEQ1_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_SEQ2:
		reg_addr_start = SIH6889_REG_SEQ2_T_DRIVER;
		break;
	case SIH6889_CONT_PARA_ASMOOTH:
		/* 6889 no ASMOOTH reg */
		return len;
	case SIH6889_CONT_PARA_TH_LEN:
		reg_addr_start = SIH6889_REG_T_0;
		break;
	case SIH6889_CONT_PARA_TH_NUM:
		reg_addr_start = SIH6889_REG_CYCLE0;
		break;
	case SIH6889_CONT_PARA_AMPLI:
		reg_addr_start = SIH6889_REG_VREF0;
		break;
	default:
		hp_err("%s:err flag %d\n", __func__, flag);
		break;
	}
	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping, reg_addr_start,
		SIH_I2C_OPERA_BYTE_THREE, reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"data1 = 0x%02x, data2 = 0x%02x, data3 = 0x%02x\n",
		reg_val[0], reg_val[1], reg_val[2]);

	return len;
}

/***********************************************
*
* chip trig config
*
***********************************************/
static void sih6889_trig_para_set(sih_haptic_t *sih_haptic, uint32_t *val)
{/* index enable polar trig_mode boost_bypass p_id n_id */
	uint8_t index = 0;
	uint8_t ctrl1 = 0;
	uint8_t ctrl2 = 0;
	uint8_t boost = 0;
	uint8_t pose_id = 0;
	uint8_t nege_id = 0;

	hp_info("%s: enter\n", __func__);
	/* trig index */
	if (val[0] < SIH_TRIG_NUM)
		index = val[0];
	else {
		hp_err("error, index = %d\n", val[0]);
		return;
	}

	/* trig enable */
	sih_haptic->trig_para[index].enable = (bool)val[1];
	ctrl1 |= (uint8_t)((bool)val[1] << index);

	/* trig polar */
	sih_haptic->trig_para[index].polar = (bool)val[2];
	ctrl1 |= (uint8_t)((bool)val[2] << (index + 4));

	/* trig mode */
	sih_haptic->trig_para[index].mode = val[3];
	ctrl2 |= val[3] << (index * 2);

	/* trig boost */
	sih_haptic->trig_para[index].boost_bypass = (bool)val[4];
	boost |= (bool)val[4] << 7;

	pose_id = val[5];
	nege_id = val[6];

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG_CTRL1, SIH6889_TRIG_CTRL1_BIT_TPOLAR0_MASK << index,
		ctrl1 & (SIH6889_TRIG_CTRL1_BIT_TPOLAR0_MASK << index));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG_CTRL1, SIH6889_TRIG_CTRL1_BIT_TRIG0_EN_MASK << index,
		ctrl1 & (SIH6889_TRIG_CTRL1_BIT_TRIG0_EN_MASK << index));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG_CTRL2, SIH6889_TRIG_CTRL2_BIT_TRIG0_MODE_MASK << index * 2,
		ctrl2 & (SIH6889_TRIG_CTRL2_BIT_TRIG0_MODE_MASK << index * 2));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG0_PACK_P + index * 2,
		SIH6889_TRIG_PACK_P_BIT_BOOST_BYPASS_MASK,
		boost & SIH6889_TRIG_PACK_P_BIT_BOOST_BYPASS_MASK);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG0_PACK_P + index * 2,
		SIH6889_TRIG_PACK_P_BIT_TRIG_PACK_MASK,
		pose_id & SIH6889_TRIG_PACK_P_BIT_TRIG_PACK_MASK);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG0_PACK_N + index * 2,
		SIH6889_TRIG_PACK_N_BIT_TRIG_PACK_MASK,
		nege_id & SIH6889_TRIG_PACK_N_BIT_TRIG_PACK_MASK);

	hp_info("%s: ctrl1 = 0x%02x, ctrl2 = 0x%02x\n", __func__, ctrl1, ctrl2);
}

static size_t sih6889_trig_para_get(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t trig_ctrl[2] = {0, 0};
	size_t len = 0;

	hp_info("%s: enter\n", __func__);

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIG_CTRL1, SIH_I2C_OPERA_BYTE_TWO, trig_ctrl);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"trig ctrl1 = 0x%02x, trig ctrl2 = 0x%02x\n", trig_ctrl[0], trig_ctrl[1]);

	return len;
}

/***********************************************
*
* chip detect config
*
***********************************************/
static void sih6889_clear_detect_done_int(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	hp_info("%s: enter\n", __func__);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINT2, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih6889_set_detect_state(sih_haptic_t *sih_haptic,
	uint8_t mode, bool flag)
{
	uint8_t f0_en;
	uint8_t f0_mask;
	bool detect_int_en = false;

	switch (mode) {
	case SIH_RAM_MODE:
		sih_haptic->detect.ram_detect_en = flag;
		f0_en = flag ? SIH6889_MODECTRL_BIT_RAM_F0_EN :
			SIH6889_MODECTRL_BIT_RAM_F0_OFF;
		f0_mask = SIH6889_MODECTRL_BIT_RAM_F0_MASK;
		break;
	case SIH_RTP_MODE:
		sih_haptic->detect.rtp_detect_en = flag;
		f0_en = flag ? SIH6889_MODECTRL_BIT_RTP_F0_EN :
			SIH6889_MODECTRL_BIT_RTP_F0_OFF;
		f0_mask = SIH6889_MODECTRL_BIT_RTP_F0_MASK;
		break;
	case SIH_TRIG_MODE:
		sih_haptic->detect.trig_detect_en = flag;
		f0_en = flag ? SIH6889_MODECTRL_BIT_TRIG_F0_EN :
			SIH6889_MODECTRL_BIT_TRIG_F0_OFF;
		f0_mask = SIH6889_MODECTRL_BIT_TRIG_F0_MASK;
		break;
	case SIH_CONT_MODE:
		sih_haptic->detect.cont_detect_en = flag;
		f0_en = flag ? SIH6889_MODECTRL_BIT_TRACK_F0_EN :
			SIH6889_MODECTRL_BIT_TRACK_F0_OFF;
		f0_mask = SIH6889_MODECTRL_BIT_TRACK_F0_MASK;
		break;
	default:
		hp_err("%s: mode parameter invalid\n", __func__);
		return;
	}

	detect_int_en =	sih_haptic->detect.cont_detect_en |
					sih_haptic->detect.trig_detect_en |
					sih_haptic->detect.rtp_detect_en |
					sih_haptic->detect.ram_detect_en;

	sih6889_detect_done_int(sih_haptic, detect_int_en);

	hp_info("%s: mask = 0x%02x, state = 0x%02x\n", __func__, f0_mask, f0_en);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAIN_STATE_CTRL, f0_mask, f0_en);
	/* clear detect done int */
	sih6889_clear_detect_done_int(sih_haptic);
}


static void sih6889_check_detect_state(sih_haptic_t *sih_haptic, uint8_t play_mode)
{
	uint8_t reg_val[2] = {0x8b, 0x36};
	bool detect_flag = false;

	hp_info("%s: enter\n", __func__);
	switch (play_mode) {
	case SIH_RAM_MODE:
	case SIH_RAM_LOOP_MODE:
		detect_flag = sih_haptic->detect.ram_detect_en;
		break;
	case SIH_RTP_MODE:
		detect_flag = sih_haptic->detect.rtp_detect_en;
		break;
	case SIH_CONT_MODE:
		detect_flag = sih_haptic->detect.cont_detect_en;
		break;
	default:
		hp_err("%s: play mode err, mode = %d\n", __func__, play_mode);
		return;
	}
	if (detect_flag) {
		haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
			SIH6889_REG_T_LAST_MONITOR_L, SIH_I2C_OPERA_BYTE_TWO, reg_val);
		sih6889_detect_fifo_ctrl(sih_haptic, false);
		sih6889_ram_init(sih_haptic, false);
		sih6889_ram_init(sih_haptic, true);
		sih6889_ram_init(sih_haptic, false);
		sih6889_detect_fifo_ctrl(sih_haptic, true);
	}
}

static size_t sih6889_get_detect_state(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t reg_val = 0;
	size_t len = 0;

	hp_info("%s: enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_MAIN_STATE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	len += snprintf(buf + len, PAGE_SIZE - len, "detect_state = %d\n", reg_val);

	return len;
}

static void sih6889_read_detect_fifo(sih_haptic_t *sih_haptic)
{
	uint8_t i;
	uint32_t f0_raw_value;
	uint32_t f0_result = 0;
	uint8_t fifo_pack = SIH6889_FIFO_PACK_SIZE;
	uint8_t fifo_raw_data[SIH6889_DETECT_FIFO_ARRAY_MAX] = {0};
	uint8_t buf[SIH6889_FIFO_PACK_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff};

	/* F0 detect read */
	for (i = 0; i < SIH6889_READ_FIFO_MAX_DATA_LEN; ++i) {
		haptic_regmap_read(sih_haptic->regmapp.regmapping,
			SIH6889_REG_RAMDATA, fifo_pack, &fifo_raw_data[i * fifo_pack]);
		if (!memcmp(buf, &fifo_raw_data[i * fifo_pack], fifo_pack))
			break;
	}

	/*F0 detect calc*/
	for (i = 2; i < SIH6889_FIFO_READ_DATA_LEN; ++i) {
		f0_raw_value =
			(uint32_t)(fifo_raw_data[i * fifo_pack + 2] << 16 |
					   fifo_raw_data[i * fifo_pack + 3] << 8  |
					   fifo_raw_data[i * fifo_pack + 4]);
		f0_result += f0_raw_value;
		hp_info("f0_raw = %d\n", f0_raw_value);
	}
	hp_info("%s: f0 result = %d\n", __func__, f0_result);
	sih_haptic->detect.detect_f0 = SIH6889_DETECT_F0_COE /
		(f0_result / SIH6889_DETECT_F0_AMPLI_COE);

	hp_info("%s: detect f0 = %d\n", __func__, sih_haptic->detect.detect_f0);
}

static void sih6889_read_tracking_f0(sih_haptic_t *sih_haptic)
{
	uint8_t data[3] = {0};
	uint32_t tracking_f0_value;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_T_HALF_TRACKING_0, SIH_I2C_OPERA_BYTE_THREE, data);
	tracking_f0_value = data[0] | data[1] << 8 | data[2] << 16;
	hp_info("%s: data[0] = 0x%02x, data[1] = 0x%02x, data[2] = 0x%02x\n",
		__func__, data[0], data[1], data[2]);
	sih_haptic->detect.tracking_f0 = SIH6889_TRACKING_F0_COE /
		tracking_f0_value;
}

#if 0
static void sih6889_read_bemf_fifo(sih_haptic_t *sih_haptic)
{
	uint8_t i;
	uint8_t fifo_pack = SIH6889_FIFO_PACK_SIZE;
	uint8_t buf[SIH6889_FIFO_PACK_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t fifo_raw_data[SIH6889_DETECT_FIFO_ARRAY_MAX] = {0};
	uint8_t bemf_result = 0;
	uint8_t reg_val = 0;

	/* F0 detect read */
	for (i = 0; i < SIH6889_FIFO_READ_DATA_LEN; ++i) {
		if (!memcmp(buf, &fifo_raw_data[i * fifo_pack], fifo_pack))
			break;
		haptic_regmap_read(sih_haptic->regmapp.regmapping,
			SIH6889_REG_RAMDATA, fifo_pack, &fifo_raw_data[i * fifo_pack]);
	}

	for (i = 1; i < SIH6889_FIFO_PACK_SIZE; ++i) {
		if (!memcmp(buf, &fifo_raw_data[i * fifo_pack], fifo_pack))
			break;

		bemf_result += (uint32_t)(fifo_raw_data[i * fifo_pack] << 8 |
			fifo_raw_data[i * fifo_pack + 1]);
	}
	bemf_result = bemf_result / (SIH6889_FIFO_PACK_SIZE - 1);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL2, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	if ((reg_val & SIH6889_SYSCTRL2_BIT_PWM_SWAP_OFF) ==
		SIH6889_SYSCTRL2_BIT_PWM_SWAP_OFF)
		sih_haptic->chip_ipara.bemf_pwm_0 = bemf_result;
	else
		sih_haptic->chip_ipara.bemf_pwm_1 = bemf_result;
}
#endif

static void sih6889_get_lra_resistance(sih_haptic_t *sih_haptic)
{
	uint8_t save_reg_addr[SIH6889_RL_CONFIG_REG_NUM] = {
		SIH6889_REG_ADC_EN_CNT,
		SIH6889_REG_ANA_CTRL1,
		SIH6889_REG_ANA_CTRL2 };
	uint8_t save_reg_val[SIH6889_RL_CONFIG_REG_NUM] = {0};
	uint8_t time = SIH6889_RL_DETECT_MAX_TRY;
	uint8_t rl_high, rl_low;
	uint64_t rl_rawdata = 0;
	uint8_t i, predef_val;

	hp_info("%s: enter\n", __func__);
	/* read regs */
	for (i = 0; i < SIH6889_RL_CONFIG_REG_NUM; i++) {
		haptic_regmap_read(sih_haptic->regmapp.regmapping,
			save_reg_addr[i], SIH_I2C_OPERA_BYTE_ONE, &save_reg_val[i]);
	}

	/* enter test mode */
	predef_val = SIH6889_TEST_ON_DBG_ON;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_LOCK_REG, SIH_I2C_OPERA_BYTE_ONE, &predef_val);
	predef_val = 0x46;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_DBG_SIGNAL_SEL2, SIH_I2C_OPERA_BYTE_ONE, &predef_val);
	predef_val = 0x70;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ANA_CTRLE, SIH_I2C_OPERA_BYTE_ONE, &predef_val);

	/* load detect rl config */
	sih6889_load_func_config(sih_haptic, REG_FUNC_RL);

	while (time--) {
		if (sih6889_if_chip_is_done(sih_haptic)) {
			usleep_range(5000, 5500);
			/* read raw data */
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH6889_REG_ADC_RL_DATA_H, SIH_I2C_OPERA_BYTE_ONE, &rl_high);
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH6889_REG_ADC_RL_DATA_L, SIH_I2C_OPERA_BYTE_ONE, &rl_low);
			rl_rawdata = (uint64_t)(rl_high & 0xf) << 8 | rl_low;

			/* calc rl */
			/*
			* rl calc formulation:
			* rl = rl_raw_data / 2048 * 1.6 / 8 / 0.42 * 1000;
			*
			* 1.6 is adc vref voltage
			* 8 is lpf amplify coefficient
			* 0.42 is rl detect current repair value
			*/
			sih_haptic->detect.resistance = rl_rawdata *
				SIH6889_RL_AMP_COE / SIH6889_RL_DIV_COE -
				sih_haptic->detect.rl_offset;
			hp_info("%s: 0x5a = 0x%02x, 0x5b = 0x%02x\n", __func__, rl_high, rl_low);
			break;
		}

		hp_info("%s: wait for done int\n", __func__);
		usleep_range(2000, 2500);
	}

	/* exit test mode */
	predef_val = SIH6889_TEST_OFF_DBG_OFF;
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_LOCK_REG, SIH_I2C_OPERA_BYTE_ONE, &predef_val);

	/* recovery regs */
	for (i = 0; i < SIH6889_RL_CONFIG_REG_NUM; i++) {
		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			save_reg_addr[i], SIH_I2C_OPERA_BYTE_ONE, &save_reg_val[i]);
	}

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RL_VBAT_CTRL, SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_MASK,
		SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_OFF);

	hp_info("detect_rl = %d\n", (uint32_t)sih_haptic->detect.resistance);
}

static void sih6889_set_ipeak(sih_haptic_t *sih_haptic, uint32_t ipeak)
{
	uint32_t val;

	switch (ipeak) {
	case 20: val = 0x0 << 0; break;
	case 25: val = 0x1 << 0; break;
	case 30: val = 0x2 << 0; break;
	case 35: val = 0x3 << 0; break;
	case 40: val = 0x4 << 0; break;
	case 45: val = 0x5 << 0; break;
	case 50: val = 0x6 << 0; break;
	case 55: val = 0x7 << 0; break;
	default: val = 0x4 << 0; break;
	}

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ANA_CTRL5, SIH6889_ANA_CTRL5_BIT_OCP_VRSEL_O_MASK, val);
}

static void sih6889_set_pvdd_by_vbat(sih_haptic_t *sih_haptic)
{
	int v_pvdd;
	int v_ipeak;
	int i;

	/* vbat range */
	if (sih_haptic->detect.vbat < SIH6889_VBAT_2_8V) {
		i = 0;
	} else if (sih_haptic->detect.vbat >= SIH6889_VBAT_2_8V &&
			sih_haptic->detect.vbat < SIH6889_VBAT_3_0V) {
		i = 1;
	} else if (sih_haptic->detect.vbat >= SIH6889_VBAT_3_0V &&
			sih_haptic->detect.vbat < SIH6889_VBAT_3_2V) {
		i = 2;
	} else
		i = 3;
	
	v_pvdd = sih_haptic->chip_attr.lp_pvdds[i];
	v_ipeak = sih_haptic->chip_attr.lp_ipeaks[i];

	hp_dbg("%s: v_pvdd = %d, v_ipeak = %d\n", __func__, v_pvdd, v_ipeak);

	sih6889_set_ipeak(sih_haptic, v_ipeak);
	sih6889_set_playback_bst_vol(sih_haptic, v_pvdd);
	sih6889_set_brk_bst_vol(sih_haptic, v_pvdd);
	sih6889_set_trig_bst_vol(sih_haptic, v_pvdd);
}

static void sih6889_get_vbat(sih_haptic_t *sih_haptic)
{
	sih6889_get_vbat_only(sih_haptic);
	sih6889_set_pvdd_by_vbat(sih_haptic);
}

static void sih6889_get_detect_f0(sih_haptic_t *sih_haptic)
{
	uint8_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;
	uint8_t cont_detect_flag = false;
	uint8_t ret = 0;

	hp_info("%s: enter\n", __func__);
	/* enter standby mode */
	sih6889_stop(sih_haptic);
	sih_haptic->detect.detect_f0 = SIH_F0_PRE_VALUE;
	sih_haptic->detect.detect_f0_read_done = false;
	if (!sih_haptic->detect.cont_detect_en) {
		sih_haptic->detect.cont_detect_en = true;
		cont_detect_flag = false;
	} else
		cont_detect_flag = true;

	/* enable detect done int */
	sih6889_clear_detect_done_int(sih_haptic);
	sih6889_detect_done_int(sih_haptic, true);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSINTM2, SIH_I2C_OPERA_BYTE_ONE, &ret);
	hp_info("%s: 0x%02x = 0x%02x\n", __func__, SIH6889_REG_SYSINTM2, ret);
	/* load detect f0 config */
	sih6889_load_func_config(sih_haptic, REG_FUNC_CONT);
	sih6889_upload_f0(sih_haptic, SIH_WRITE_ZERO);
	/* detect config */
	sih6889_detect_fifo_ctrl(sih_haptic, false);
	sih6889_ram_init(sih_haptic, false);
	sih6889_ram_init(sih_haptic, true);
	sih6889_ram_init(sih_haptic, false);
	sih6889_detect_fifo_ctrl(sih_haptic, true);
	sih6889_f0_tracking(sih_haptic, true);
	/* play go */
	sih6889_set_play_mode(sih_haptic, SIH_CONT_MODE);
	sih6889_play_go(sih_haptic, true);
	/* wait for read done */
	while (cnt--) {
		if (sih_haptic->detect.detect_f0_read_done)
			break;
		usleep_range(2000, 2500);
	}
	sih_haptic->detect.cont_detect_en = cont_detect_flag;
	/* recovery done int state */
	if (sih_haptic->detect.rtp_detect_en | sih_haptic->detect.cont_detect_en |
		sih_haptic->detect.ram_detect_en | sih_haptic->detect.trig_detect_en)
		sih6889_detect_done_int(sih_haptic, true);
	else 
		sih6889_detect_done_int(sih_haptic, false);

	hp_info("f0 = %d\n", sih_haptic->detect.detect_f0);
}

static void sih6889_get_tracking_f0(sih_haptic_t *sih_haptic)
{
	uint8_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;

	hp_info("%s: enter\n", __func__);
	sih6889_stop(sih_haptic);
	sih_haptic->detect.tracking_f0 = SIH_F0_PRE_VALUE;
	/* disable auto n*/
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RL_VBAT_CTRL, SIH6889_RL_VBAT_CTRL_BIT_AUTO_N_CAL_EN_MASK,
		SIH6889_RL_VBAT_CTRL_BIT_AUTO_N_CAL_OFF);
	/* load tracking f0 config */
	sih6889_load_func_config(sih_haptic, REG_FUNC_CONT);
	sih6889_upload_f0(sih_haptic, SIH_WRITE_ZERO);
	sih6889_f0_tracking(sih_haptic, false);
	/* play go */
	sih6889_set_play_mode(sih_haptic, SIH_CONT_MODE);
	sih6889_play_go(sih_haptic, true);
	/* wait for standby */
	while (cnt--) {
		if (sih6889_if_chip_is_mode(sih_haptic, SIH_IDLE_MODE))
			break;
		usleep_range(2000, 2500);
	}
	/* read f0 data */
	sih6889_read_tracking_f0(sih_haptic);
	hp_info("tracking_f0 = %d\n", sih_haptic->detect.tracking_f0);

	/* enable auto n*/
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RL_VBAT_CTRL, SIH6889_RL_VBAT_CTRL_BIT_AUTO_N_CAL_EN_MASK,
		SIH6889_RL_VBAT_CTRL_BIT_AUTO_N_CAL_EN);
}

/***********************************************
*
* chip cali config
*
***********************************************/
static void sih6889_osc_cali(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;
	int32_t code = 0;
	int32_t tmp = 0;

	hp_info("%s: enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_PWM_UP_SAMPLE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	switch (reg_val & SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK) {
	case SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_ONE_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH6889_OSC_RTL_DATA_LEN /
				SIH6889_PWM_SAMPLE_48KHZ;
		break;
	case SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_TWO_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH6889_OSC_RTL_DATA_LEN /
				SIH6889_PWM_SAMPLE_24KHZ;
		break;
	case SIH6889_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_FOUR_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH6889_OSC_RTL_DATA_LEN /
				SIH6889_PWM_SAMPLE_12KHZ;
		break;
	default:
		break;
	}

	hp_info("%s: actual_time = %d, theory_time = %d\n", __func__,
		sih_haptic->osc_para.actual_time, sih_haptic->osc_para.theory_time);

	tmp = (int32_t)((int64_t)sih_haptic->osc_para.actual_time *
		SIH6889_OSC_CALI_COE / sih_haptic->osc_para.theory_time);

	code = (tmp - SIH6889_OSC_CALI_COE) * SIH6889_OSC_CALI_COE /
		SIH6889_F0_DELTA;

	sih_haptic->osc_para.osc_data = (uint8_t)code;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_TRIM1, SIH_I2C_OPERA_BYTE_ONE,
		&sih_haptic->osc_para.osc_data);
}

#if 0
static void __maybe_unused sih6889_get_bemf_value(sih_haptic_t *sih_haptic)
{
	sih6889_ram_init(sih_haptic, true);
	sih6889_detect_fifo_ctrl(sih_haptic, true);
	/* read f0 data */
	sih6889_read_bemf_fifo(sih_haptic);

	sih6889_ram_init(sih_haptic, false);
	sih6889_detect_fifo_ctrl(sih_haptic, false);
}
#endif

static void __maybe_unused sih6889_set_pwm_swap(sih_haptic_t *sih_haptic, bool flag)
{
	uint32_t val = flag ? SIH6889_SYSCTRL2_BIT_PWM_SWAP_EN :
		SIH6889_SYSCTRL2_BIT_PWM_SWAP_OFF;
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL2, SIH6889_SYSCTRL2_BIT_PWM_SWAP_MASK, val);
}

#if 0
/**
 * @brief set broadcast addr.
*/
static void sih6889_rtp_brd_set_addr(sih_haptic_t *sih_haptic, uint8_t brd_addr)
{
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_BRDCAST_SLV_ADDR, SIH_I2C_OPERA_BYTE_ONE, &brd_addr);
	sih_haptic->rtp.brd_slv_addr = brd_addr;
}

/**
 * @brief get broadcast addr.
*/
static void sih6889_rtp_brd_get_addr(sih_haptic_t *sih_haptic)
{
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_BRDCAST_SLV_ADDR, SIH_I2C_OPERA_BYTE_ONE,
		&sih_haptic->rtp.brd_slv_addr);
}
#endif

static void sih6889_init(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;
	// int i;

	// /* init default i2c slave device addr */
	// sih_haptic->chip_attr.default_dev_addr = sih_haptic->i2c->addr;
	// hp_info("%s: i2c device addr = 0x%02x, default_dev_addr = 0x%02x\n",
	// 	__func__, sih_haptic->i2c->addr, sih_haptic->chip_attr.default_dev_addr);

	/* idle cnt reg init */
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH6889_REG_IDLE_DEL_CNT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	/* gain init */
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_PWM_PRE_GAIN, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	sih_haptic->chip_ipara.gain = reg_val;

	/* wait detect fifo off */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SMOOTH_F0_WINDOW_OUT_DET_CTRL,
		SIH6889_WAIT_FIFO_DETECT_MASK, SIH6889_WAIT_FIFO_DETECT_OFF);

	/* brk vboost init */
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_V_BOOST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	/**
	* brk vboost calc formulation:
	* 1bit = 0.0625v
	* brk vboost has increased by ten times
	*/
	sih_haptic->chip_ipara.brk_vboost = (((uint32_t)reg_val) << 8) *
		SIH6889_BRK_VBOOST_COE / SIH6889_VBOOST_MUL_COE;
	sih6889_set_brk_bst_vol(sih_haptic, sih_haptic->chip_ipara.brk_vboost);

	// /* trig vboost init */
	// sih_haptic->chip_ipara.trig_vboost = SIH_DRIVER_VBOOST_INIT_VALUE;
	// sih6889_set_trig_bst_vol(sih_haptic, sih_haptic->chip_ipara.trig_vboost);

	/* ram wave init */
	sih_haptic->ram.lib_index = SIH_INIT_ZERO_VALUE;

	/* f0 pre init */
	sih_haptic->detect.detect_f0 = sih_haptic->detect.cali_target_value;
	sih_haptic->detect.tracking_f0 = sih_haptic->detect.cali_target_value;

	/* osc data init */
	sih_haptic->osc_para.osc_data = SIH_INIT_ZERO_VALUE;
	/* rl pre init */
	sih_haptic->detect.resistance = SIH_INIT_ZERO_VALUE;
	sih_haptic->detect.rl_offset = SIH6889_RL_OFFSET;

	/* vbat init */
	sih_haptic->detect.vbat = SIH_INIT_ZERO_VALUE;

	/* rtp start thres init */
	sih_haptic->rtp.rtp_start_thres = SIH_RTP_START_DEFAULT_THRES;

	/* state init */
	sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;

	/* drv vboost init */
	sih6889_set_drv_bst_vol(sih_haptic, sih_haptic->chip_ipara.drv_vboost);

	// /* lp_pvdd & lp_ipeak init */
	// for (i = 0; i < SIH_LP_INDEX; i++) {
	// 	sih_haptic->chip_attr.lp_pvdds[i] = s_lp_pvdds[i];
	// 	sih_haptic->chip_attr.lp_ipeaks[i] = s_lp_ipeaks[i];
	// }

	/* low power mode init */
	sih6889_set_low_power_mode(sih_haptic, true);

	/* detect state init */
	sih_haptic->detect.ram_detect_en = SIH_INIT_ZERO_VALUE;
	sih_haptic->detect.rtp_detect_en = SIH_INIT_ZERO_VALUE;
	sih_haptic->detect.trig_detect_en = SIH_INIT_ZERO_VALUE;
	sih_haptic->detect.cont_detect_en = SIH_INIT_ZERO_VALUE;

	/* update chip para */
	sih6889_get_vbat(sih_haptic);
	sih6889_get_lra_resistance(sih_haptic);

	// /* broadcast address init */
	// if (sih_haptic->rtp.brd_slv_addr)
	// 	sih6889_rtp_brd_set_addr(sih_haptic, sih_haptic->rtp.brd_slv_addr);
	// else
	// 	sih6889_rtp_brd_get_addr(sih_haptic);
}
#if 0
static void sih6889_get_polar_state(sih_haptic_t *sih_haptic, uint8_t *reg_val)
{
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL2, SIH_I2C_OPERA_BYTE_ONE, reg_val);
	hp_info("%s: 0x06 = 0x%02x\n", __func__, *reg_val);
}

static void sih6889_set_polar_state(sih_haptic_t *sih_haptic, uint8_t reg_val)
{
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_SYSCTRL2, SIH6889_SYSCTRL2_BIT_PWM_SWAP_MASK, reg_val);
	hp_info("%s: 0x06 = 0x%02x\n", __func__, reg_val);
}

static int sih6889_audio_start(sih_haptic_t *sih_haptic)
{
	return 0;
}

static int sih6889_audio_mute(sih_haptic_t *sih_haptic)
{
	return 0;
}

static int sih6889_get_hwinfo(sih_haptic_t *sih_haptic)
{
	return SIH6889_PLATFORM_HWINFO;
}

static void sih6889_get_chip_id(sih_haptic_t *sih_haptic, uint8_t *reg_val)
{
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH6889_REG_ID, SIH_I2C_OPERA_BYTE_ONE, reg_val);
}

static void sih6889_get_reg_nums(sih_haptic_t *sih_haptic, uint8_t *reg_nums)
{
	*reg_nums = SIH6889_REG_MAX;
}

/**
 * @brief return sih_haptic device i2s register address and nums.
 * @param sih_haptic_t* sih_haptic, haptic device
 * @param uint8_t** param_addrs, used to store i2s regs address
 * @param uint8_t* reg_nums, used to store i2s regs nums
 * @retval void
*/
static void sih6889_get_iis_param(sih_haptic_t *sih_haptic, uint8_t **param_addrs,
	uint8_t *reg_nums)
{
	*param_addrs = sih6889_iis_addrs;
	*reg_nums = ARRAY_LEN(sih6889_iis_addrs);
}

/**
 * @brief whether the sih_haptic chip has i2s module.
 * @retval bool
 * 				-- true, with i2s module;
 * 				-- false, without i2s module;
*/
static bool sih6889_check_iis(sih_haptic_t *sih_haptic)
{
	return true;
}

// /**
//  * @brief iic bus writes data to chips in broadcast mode, and the data is
//  * organized as channel parity.
//  * @param	sih_haptic_t* sih_haptic
//  * @param	uint8_t* data0, data will be wrote to chip 0
//  * @param	uint32_t len0, data0 length
//  * @param	uint8_t* data1, data will be wrote to chip 1
//  * @param	uint32_t len1, data1 length
//  * @param	uint8_t* data, data will be send by iic bus
//  * @param	uint32_t len, data length
//  * @retval	int, when errors occur, return -1, else return real length of data. 
// */
// static int sih_iic_broadcast_prepare_datas_with_channel_parity(sih_haptic_t
// 	*sih_haptic, uint8_t *data0, uint32_t len0, uint8_t *data1, uint32_t len1,
// 	uint8_t *data, uint32_t len)
// {
// 	uint32_t i;
// 	int count;

// 	if ((len0 + len1 > len) || data0 == NULL ||
// 		data1 == NULL || data == NULL)
// 	{
// 		hp_err("%s, bad param!\n", __func__);
// 		return -EINVAL;
// 	}
	
// 	/* even index datas */
// 	for (i = 0; i < len0; i++)
// 		data[i*2] = data0[i];

// 	count += len0;

// 	/* odd index datas */
// 	for (i = 0; i < len1; i++)
// 		data[i*2+1] = data1[i];

// 	count += len1;
	
// 	return count;
// }

/**
 * @brief update given bits
*/
static inline void sih6889_update_bit(uint8_t *val, uint8_t data, uint8_t mask)
{
	*val &= ~mask;
	*val |= data & mask;
}

/**
 * @brief reorganize two datas with interweave mode, and store in output_data.
*/
static void sih6889_interweave_data(const uint8_t data0, const uint8_t data1,
	uint8_t *output_data)
{
	uint8_t i;
	uint8_t odd, even;
	uint8_t tmp_data0, tmp_data1;
	uint8_t mask;

	for (i = 0; i < 8; i++) {
		tmp_data0 = data0;
		tmp_data1 = data1;
		
		/* get bit */
		odd = (tmp_data1 >> (7-i) & 0x1);
		even = (tmp_data0 >> (7-i) & 0x1);
		/* store bits to corresponding place */
		if (i < 4) {
			mask = 0x1 << (7-2*i);
			sih6889_update_bit(&output_data[0], odd << (7-2*i), mask);
			mask = 0x1 << (7-2*i-1);
			sih6889_update_bit(&output_data[0], even << (7-2*i-1), mask);
		} else {
			mask = 0x1 << (15-2*i);
			sih6889_update_bit(&output_data[1], odd << (15-2*i), mask);
			mask = 0x1 << (15-2*i-1);
			sih6889_update_bit(&output_data[1], even << (15-2*i-1), mask);
		}
	}
}

/**
 * @brief iic bus writes data to chips in broadcast mode, and the data
 * is organized as interweave.
 * @param	sih_haptic_t* sih_haptic
 * @param	uint8_t* data0, data will be wrote to chip 0
 * @param	uint32_t len0, data0 length
 * @param	uint8_t* data1, data will be wrote to chip 1
 * @param	uint32_t len1, data1 length
 * @param	uint8_t* data, data will be send by iic bus
 * @param	uint32_t len, data length
 * @retval	int, when errors occur, return -1, else return real length of data. 
*/
static int sih_iic_broadcast_prepare_datas_with_interweave(sih_haptic_t
	*sih_haptic, uint8_t *data0, uint32_t len0, uint8_t *data1, uint32_t len1,
	uint8_t *data, uint32_t len)
{
	uint32_t i;
	int count;
	uint32_t min_len;
	uint32_t index;
	uint8_t *padding_data = NULL;
	uint32_t padding_len;

	if (!data0 || !data1 || !data || (len0 + len1 > len) ) {
		hp_err("%s, invalid param!\n", __func__);
		return -EINVAL;
	}

	min_len = len0 < len1 ? len0 : len1;

	for (i = 0; i < min_len; i++)
		sih6889_interweave_data(data0[i], data1[i], &data[2*i]);

	count = 2 * min_len;
	index = i;
	
	hp_dbg("%s: len1 = %d, len2 = %d, len = %d, count = %d\n", __func__, len0, len1, len, count);

	if (len0 == len1)
		return count;
	
	/** 
	 * if chip0 and chip1 want to receive different data lengths.
	 * NEED TO the process the rest of data, and padding data set to 0.
	*/
	if (len0 < len1)
		padding_len = len1 - len0;
	else
		padding_len = len0 - len1;

	/* malloc padding data, and set value to 0 */
	padding_data = (uint8_t *)vmalloc(padding_len);
	if (!padding_data) {
		hp_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	} else
		memset(padding_data, 0, padding_len);

	for (i = 0; i < padding_len; i++) {
		if (len0 < len1)
			sih6889_interweave_data(padding_data[i], data1[i+index], &data[2*(i+index)]);
		else
			sih6889_interweave_data(data0[i+index], padding_data[i], &data[2*(i+index)]);
	}

	vfree(padding_data);
	count += 2 * padding_len;

	return count;
}

/**
 * @brief 	sih6889 broadcast datas by iic bus, and the data is organized as interweave.
 * @param	sih_haptic_t* sih_haptic.
 * @param	uint8_t* data0, data will be wrote to chip 0.
 * @param	uint32_t len0, data0 length.
 * @param	uint8_t* data1, data will be wrote to chip 1.
 * @param	uint32_t len1, data1 length.
 * @param	uint8_t* data, data will be send by iic bus.
 * @param	uint32_t len, data length. 
 * @retval	int, when errors occur, return -1, else return real length of data.
*/
static int sih6889_iic_broadcast_prepare_datas(sih_haptic_t *sih_haptic,
	uint8_t *data0, uint32_t len0, uint8_t *data1, uint32_t len1,
	uint8_t *data, uint32_t len)
{
	return sih_iic_broadcast_prepare_datas_with_interweave(sih_haptic,
				data0, len0, data1, len1, data, len);
}

static int sih6889_set_regs_val(sih_haptic_t *sih_haptic, uint32_t *reg_addr,
	uint32_t *reg_value, uint32_t len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		hp_dbg("%s: reg_addr = 0x%02x, reg_val = 0x%02x\n",
			__func__, reg_addr[i], reg_value[i]);
		haptic_regmap_write(sih_haptic->regmapp.regmapping, reg_addr[i],
			SIH_I2C_OPERA_BYTE_ONE, (uint8_t *)&reg_value[i]);
	}

	sih6889_save_cont_config(sih_haptic, reg_addr, reg_value, len);

	return 0;
}

static void sih6889_set_rtp_i2s_bus(sih_haptic_t *sih_haptic, bool on)
{
	hp_info("%s: on = %s\n", __func__, on ? "true" : " false");
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTP_IIS_CFG, SIH6889_RTP_IIS_CFG_BIT_IIS_ENABLE_MASK,
		on ? SIH6889_RTP_IIS_CFG_BIT_IIS_ENABLE :
		SIH6889_RTP_IIS_CFG_BIT_IIS_DISABLE);
}

static void sih6889_set_rtp_brd(sih_haptic_t *sih_haptic, bool on)
{
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTP_BRDCAST_CTRL, SIH6889_BROADCAST_BIT_RTP_BROADCASR_EN_MASK,
		on ? SIH6889_BROADCAST_BIT_RTP_BROADCASR_EN :
		SIH6889_BROADCAST_BIT_RTP_BROADCASR_DISABLE);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH6889_REG_RTP_BRDCAST_CTRL, SIH6889_BROADCAST_BIT_CHL_SEL_MASK,
		on ? SIH6889_BROADCAST_BIT_CHL_SEL_EN :
		SIH6889_BROADCAST_BIT_CHL_SEL_OFF);
}

static void sih6889_set_clk_force_sync(sih_haptic_t *sih_haptic, bool on)
{
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
	SIH6889_REG_RTP_IIS_CFG, SIH6889_RTP_IIS_CFG_BIT_FORCE_SYNC_MASK,
	on ? SIH6889_RTP_IIS_CFG_BIT_FORCE_SYNC_EN :
	SIH6889_RTP_IIS_CFG_BIT_FORCE_SYNC_OFF);
}
#endif

static void sih6889_unlock(sih_haptic_t *sih_haptic)
{
	int ret = -1;
	uint8_t reg_val = SIH6889_UNLOCK;

	ret = i2c_write_bytes(sih_haptic, SIH6889_REG_UNLOCK, &reg_val, SIH_I2C_OPERA_BYTE_ONE);
	if (ret < 0)
		hp_err("%s: i2c_addr = 0x%02x, write reg(0x%02x) err\n",
			__func__, sih_haptic->i2c->addr, SIH6889_REG_UNLOCK);
}

haptic_func_t sih_6889_func_list = {
	.probe = sih6889_probe,
	.init = sih6889_init,
	.ram_init = sih6889_ram_init,
	.detect_fifo_ctrl = sih6889_detect_fifo_ctrl,
	.interrupt_state_init = sih6889_interrupt_state_init,
	.chip_software_reset = sih6889_software_reset,
	.chip_hardware_reset = sih6889_hardware_reset,
	.update_chip_state = sih6889_update_chip_state,
	.update_ram_config = sih6889_update_ram_config,
	.clear_interrupt_state = sih6889_clear_interrupt_state,
	.play_go = sih6889_play_go,
	.stop = sih6889_stop,
	.get_vbat = sih6889_get_vbat,
	.get_detect_f0 = sih6889_get_detect_f0,
	.get_tracking_f0 = sih6889_get_tracking_f0,
	.get_lra_resistance = sih6889_get_lra_resistance,
	.set_play_mode = sih6889_set_play_mode,
	.set_drv_bst_vol = sih6889_set_drv_bst_vol,
	.set_brk_bst_vol = sih6889_set_brk_bst,
	.set_repeat_seq = sih6889_set_repeat_seq,
	.get_wav_seq = sih6889_get_wav_seq,
	.set_wav_seq = sih6889_set_wav_seq,
	.get_wav_loop = sih6889_get_wav_loop,
	.set_wav_loop = sih6889_set_wav_loop,
	.set_wav_main_loop = sih6889_set_wav_main_loop,
	.get_wav_main_loop = sih6889_get_wav_main_loop,
	.get_first_wave_addr = sih6889_get_first_wave_addr,
	.get_ram_data = sih6889_get_ram_data,
	.set_ram_addr = sih6889_set_ram_addr,
	.set_rtp_aei = sih6889_set_rtp_aei,
	.set_start_thres = sih6889_start_thres,
	.write_rtp_data = sih6889_write_rtp_data,
	.if_chip_is_mode = sih6889_if_chip_is_mode,
	.if_chip_is_detect_done = sih6889_if_chip_is_detect_done,
	.get_rtp_fifo_empty_state = sih6889_get_rtp_fifo_empty_state,
	.get_rtp_fifo_full_state = sih6889_get_rtp_fifo_full_state,
	.set_gain = sih6889_set_gain,
	.vbat_comp = sih6889_vbat_comp,
	.upload_f0 = sih6889_upload_f0,
	.set_boost_mode = sih6889_set_boost_mode,
	.set_auto_pvdd = sih6889_set_auto_pvdd,
	.set_low_power_mode = sih6889_set_low_power_mode,
	.get_trig_para = sih6889_trig_para_get,
	.set_trig_para = sih6889_trig_para_set,
	.get_ram_seq_gain = sih6889_get_ram_seq_gain,
	.set_ram_seq_gain = sih6889_set_ram_seq_gain,
	.get_brk_state = sih6889_get_brk_state,
	.set_brk_state = sih6889_set_brk_state,
	.get_detect_state = sih6889_get_detect_state,
	.set_detect_state = sih6889_set_detect_state,
	.check_detect_state = sih6889_check_detect_state,
	.get_pwm_rate = sih6889_get_pwm_rate,
	.set_pwm_rate = sih6889_set_pwm_rate,
	.osc_cali = sih6889_osc_cali,
	.efuse_check = sih6889_efuse_check,
	.read_detect_fifo = sih6889_read_detect_fifo,
	.get_cont_para = sih6889_get_cont_para,
	.set_cont_para = sih6889_set_cont_para,
};
