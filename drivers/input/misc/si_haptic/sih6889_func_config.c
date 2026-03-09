/*
 *  Silicon Integrated Co., Ltd haptic sih6889 haptic config source file
 *
 *  Copyright (c) 2024 shanfa <shanfa.tang@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <asm/ioctls.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/firmware.h>

#include "sih6889_func_config.h"

reg_format_t sih6889_lra_9595_config_list[] = {
	{SIH6889_REG_T_LAST_MONITOR_L,			0x8B,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_LAST_MONITOR_H,			0x36,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF0,						0x4B,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF1,						0x16,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF2,						0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE0,					0x04,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE1,					0x3c,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE2,					0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_WATCHDOG_CNT_MAX,			0x24,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_0,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_1,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_2,						0x11,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_0,	0xcd,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_1,	0x04,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_2,	0x00,	0xff,	OPERATION_WRITE},
	{0,									0,		0,		OPERATION_END},
};

reg_format_t sih6889_lra_0809_config_list[] = {
	{SIH6889_REG_T_LAST_MONITOR_L,			0x8B,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_LAST_MONITOR_H,			0x36,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF0,						0x46,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF1,						0x1b,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF2,						0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE0,					0x04,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE1,					0x3c,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE2,					0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_WATCHDOG_CNT_MAX,			0x24,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_0,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_1,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_2,						0x11,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_0,	0xcd,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_1,	0x04,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_2,	0x00,	0xff,	OPERATION_WRITE},
	{0,									0,		0,		OPERATION_END},
};

reg_format_t sih6889_lra_0815_config_list[] = {
	{SIH6889_REG_T_LAST_MONITOR_L,			0x8B,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_LAST_MONITOR_H,			0x36,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF0,						0x32,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF1,						0x14,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_VREF2,						0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE0,					0x04,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE1,					0x3c,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_CYCLE2,					0x00,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_WATCHDOG_CNT_MAX,			0x24,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ0_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ1_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_DRIVER,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_FLUSH,				0x06,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_SEQ2_T_BEMF,				0x1a,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_0,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_1,						0xa5,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_T_2,						0x11,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_0,	0xcd,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_1,	0x04,	0xff,	OPERATION_WRITE},
	// {SIH6889_REG_SMOOTH_CONST_ALGO_DATA_2,	0x00,	0xff,	OPERATION_WRITE},
	{0,									0,		0,		OPERATION_END},
};

reg_format_t sih6889_detect_rl_config_list[] = {
	{SIH6889_REG_ADC_EN_CNT,			0x40,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_ANA_CTRL1,				0x09,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_ANA_CTRL2,				0x35,	0xff,	OPERATION_WRITE},
	{SIH6889_REG_RL_VBAT_CTRL,			SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_EN,
		SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_MASK,	OPERATION_BIT},
	{SIH6889_REG_RL_VBAT_CTRL,			SIH6889_RL_VBAT_CTRL_BIT_DET_GO_EN,
		SIH6889_RL_VBAT_CTRL_BIT_DET_GO_MASK,	OPERATION_BIT},
	{0,									0,		0,		OPERATION_END},
};

reg_format_t sih6889_detect_vbat_config_list[] = {
	{SIH6889_REG_RL_VBAT_CTRL,			SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_OFF,
		SIH6889_RL_VBAT_CTRL_BIT_DET_MODE_MASK,	OPERATION_BIT},
	{SIH6889_REG_RL_VBAT_CTRL,			SIH6889_RL_VBAT_CTRL_BIT_DET_GO_EN,
		SIH6889_RL_VBAT_CTRL_BIT_DET_GO_MASK,	OPERATION_BIT},
	{0,									0,		0,		OPERATION_END},
};


haptic_bin_file_reg_format_t sih6889_common_reg_conts[] = {
	{0x40, 0xfe},
	{0x45, 0x1c},
	{0x68, 0x48},
	{0x85, 0x05},
	{0xa7, 0x71},
	{0xa8, 0x1c},
	{0xa9, 0x00},
	{0xb3, 0x50},
	{0xb4, 0x0b},
	{0xb5, 0x00},
	{0xe7, 0xd8},
	{0xe8, 0xd4},
	{0xe9, 0xff},
	{0xea, 0xf8},
	{0xeb, 0x1a},
	{0xec, 0x00},
	{0xed, 0x01},
	{0xee, 0x16},
	{0xef, 0x00},
	{0xf0, 0xe2},
	{0xf1, 0x02},
	{0xf2, 0x00},
	{0xf3, 0x46},
	{0xf4, 0x20},
	{0xf5, 0x00},
	{0xf6, 0x0c},
	{0xf7, 0xfe},
	{0xf8, 0xff},
};

/************************************************************************/
/*																		*/
/*					sih6889 9595 default reg config						*/
/*																		*/
/************************************************************************/
haptic_bin_file_reg_format_t sih6889_9595_lra_reg_conts[] = {
	{0x0c, 0x00},
	{0x34, 0x23},
	{0x42, 0x05},
	{0x82, 0x24},
	{0x83, 0x8b},
	{0x84, 0x36},
	{0x8f, 0xa0},
	{0x90, 0x0f},
	{0x91, 0x00},
	{0x92, 0x95},
	{0x93, 0x0b},
	{0x94, 0x00},
	{0x95, 0x99},
	{0x96, 0x08},
	{0x97, 0x02},
	{0x9b, 0xcc},
	{0x9c, 0x04},
	{0x9d, 0x00},
	{0x9e, 0x99},
	{0x9f, 0x01},
	{0xa0, 0x00},
	{0xa1, 0x65},
	{0xa2, 0x73},
	{0xa3, 0xfe},
	{0xa4, 0x76},
	{0xa5, 0x10},
	{0xa6, 0x02},
	{0xaa, 0xa5},
	{0xab, 0xa5},
	{0xac, 0x11},
	{0xb0, 0x03},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb6, 0x67},
	{0xb7, 0x15},
	{0xb8, 0x00},
	{0xb9, 0x80},
	{0xba, 0xcc},
	{0xbb, 0x04},
	{0xbc, 0x00},
	{0xbd, 0xcc},
	{0xbe, 0x04},
	{0xbf, 0x00},
	{0xc2, 0x04},
	{0xc8, 0x14},
	{0xc9, 0x00},
	{0xca, 0x00},
	{0xcb, 0x24},
	{0xcc, 0x1a},
	{0xcd, 0x06},
	{0xce, 0x1a},
	{0xcf, 0x1a},
	{0xd0, 0x06},
	{0xd1, 0x1a},
	{0xd2, 0x1a},
	{0xd3, 0x06},
	{0xd4, 0x1a},
	{0xd5, 0x01},
	{0xd6, 0x07},
	{0xd7, 0x0c},
	{0xd8, 0x01},
	{0xd9, 0x0d},
	{0xe3, 0x80},
	{0xe4, 0x00},
	{0xe5, 0x10},
	{0xe6, 0x00},
	{0xf9, 0x00},
	{0xfa, 0x08},
	{0xfb, 0x00},
};

haptic_bin_file_reg_format_t sih6889_9595_flexible_reg_conts[] = {
	{0x80, 0xe0},
	{0x81, 0x01},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x00},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0x8c, 0xae},
	{0x8d, 0x02},
	{0x8e, 0x00},
	{0x98, 0x33},
	{0x99, 0x0b},
	{0x9a, 0x00},
	{0xad, 0xd7},
	{0xae, 0x0e},
	{0xaf, 0x00},
	{0xc0, 0x00},
	{0xc3, 0x66},
	{0xc4, 0x02},
	{0xc5, 0x00},
	{0xe0, 0xad},
	{0xe1, 0x01},
	{0xe2, 0x00},
};

haptic_reg_config_group_t sih6889_9595_dedault_reg_config_list = {
	.common = {
		.reg_nums = ARRAY_LEN(sih6889_common_reg_conts),
		.reg_conts = sih6889_common_reg_conts
	},
	.flexible = {
		.reg_nums = ARRAY_LEN(sih6889_9595_flexible_reg_conts),
		.reg_conts = sih6889_9595_flexible_reg_conts
	},
	.lra = {
		.reg_nums = ARRAY_LEN(sih6889_9595_lra_reg_conts),
		.reg_conts = sih6889_9595_lra_reg_conts
	},
};

/************************************************************************/
/*																		*/
/*					sih6889 0809 default reg config						*/
/*																		*/
/************************************************************************/
haptic_bin_file_reg_format_t sih6889_0809_lra_reg_conts[] = {
	{0x0c, 0x00},
	{0x34, 0x23},
	{0x42, 0x05},
	{0x82, 0x24},
	{0x83, 0x8b},
	{0x84, 0x36},
	{0x8f, 0xb1},
	{0x90, 0x0f},
	{0x91, 0x00},
	{0x92, 0x88},
	{0x93, 0x0b},
	{0x94, 0x00},
	{0x95, 0x4f},
	{0x96, 0x06},
	{0x97, 0x02},
	{0x9b, 0xcc},
	{0x9c, 0x04},
	{0x9d, 0x00},
	{0x9e, 0x99},
	{0x9f, 0x01},
	{0xa0, 0x00},
	{0xa1, 0x90},
	{0xa2, 0x18},
	{0xa3, 0xfe},
	{0xa4, 0x41},
	{0xa5, 0x8b},
	{0xa6, 0x02},
	{0xaa, 0xa5},
	{0xab, 0xa5},
	{0xac, 0x11},
	{0xb0, 0x03},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb6, 0x7f},
	{0xb7, 0x15},
	{0xb8, 0x00},
	{0xb9, 0x80},
	{0xba, 0xcc},
	{0xbb, 0x04},
	{0xbc, 0x00},
	{0xbd, 0xcc},
	{0xbe, 0x04},
	{0xbf, 0x00},
	{0xc2, 0x04},
	{0xc8, 0x14},
	{0xc9, 0x00},
	{0xca, 0x00},
	{0xcb, 0x24},
	{0xcc, 0x1a},
	{0xcd, 0x06},
	{0xce, 0x1a},
	{0xcf, 0x1a},
	{0xd0, 0x06},
	{0xd1, 0x1a},
	{0xd2, 0x1a},
	{0xd3, 0x06},
	{0xd4, 0x1a},
	{0xd5, 0x01},
	{0xd6, 0x07},
	{0xd7, 0x0c},
	{0xd8, 0x01},
	{0xd9, 0x0d},
	{0xe3, 0xb3},
	{0xe4, 0x00},
	{0xe5, 0x10},
	{0xe6, 0x00},
	{0xf9, 0x00},
	{0xfa, 0x08},
	{0xfb, 0x00},
};

haptic_bin_file_reg_format_t sih6889_0809_flexible_reg_conts[] = {
	{0x80, 0xe0},
	{0x81, 0x01},
	{0x86, 0x46},
	{0x87, 0x1b},
	{0x88, 0x00},
	{0x89, 0x04},
	{0x8a, 0x3c},
	{0x8b, 0x00},
	{0x8c, 0x7c},
	{0x8d, 0x02},
	{0x8e, 0x00},
	{0x98, 0x00},
	{0x99, 0x0c},
	{0x9a, 0x00},
	{0xad, 0x4e},
	{0xae, 0x13},
	{0xaf, 0x00},
	{0xc0, 0x05},
	{0xe0, 0x8d},
	{0xe1, 0x01},
	{0xe2, 0x00},
};

haptic_reg_config_group_t sih6889_0809_dedault_reg_config_list = {
	.common = {
		.reg_nums = ARRAY_LEN(sih6889_common_reg_conts),
		.reg_conts = sih6889_common_reg_conts
	},
	.flexible = {
		.reg_nums = ARRAY_LEN(sih6889_0809_flexible_reg_conts),
		.reg_conts = sih6889_0809_flexible_reg_conts
	},
	.lra = {
		.reg_nums = ARRAY_LEN(sih6889_0809_lra_reg_conts),
		.reg_conts = sih6889_0809_lra_reg_conts
	}
};

/************************************************************************/
/*																		*/
/*					sih6889 0815 default reg config						*/
/*																		*/
/************************************************************************/
haptic_bin_file_reg_format_t sih6889_0815_lra_reg_conts[] = {
	{0x0c, 0x00},
	{0x34, 0x23},
	{0x42, 0x05},
	{0x82, 0x24},
	{0x83, 0x8b},
	{0x84, 0x36},
	{0x8f, 0xa3},
	{0x90, 0x0f},
	{0x91, 0x00},
	{0x92, 0x93},
	{0x93, 0x0b},
	{0x94, 0x00},
	{0x95, 0x30},
	{0x96, 0x08},
	{0x97, 0x02},
	{0x9b, 0xcc},
	{0x9c, 0x04},
	{0x9d, 0x00},
	{0x9e, 0x99},
	{0x9f, 0x01},
	{0xa0, 0x00},
	{0xa1, 0x90},
	{0xa2, 0x5f},
	{0xa3, 0xfe},
	{0xa4, 0xcb},
	{0xa5, 0x2a},
	{0xa6, 0x02},
	{0xaa, 0xa5},
	{0xab, 0xa5},
	{0xac, 0x11},
	{0xb0, 0x03},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb6, 0x6b},
	{0xb7, 0x15},
	{0xb8, 0x00},
	{0xb9, 0x80},
	{0xba, 0xcc},
	{0xbb, 0x04},
	{0xbc, 0x00},
	{0xbd, 0xcc},
	{0xbe, 0x04},
	{0xbf, 0x00},
	{0xc2, 0x04},
	{0xc8, 0x14},
	{0xc9, 0x00},
	{0xca, 0x00},
	{0xcb, 0x24},
	{0xcc, 0x1a},
	{0xcd, 0x06},
	{0xce, 0x1a},
	{0xcf, 0x1a},
	{0xd0, 0x06},
	{0xd1, 0x1a},
	{0xd2, 0x1a},
	{0xd3, 0x06},
	{0xd4, 0x1a},
	{0xd5, 0x01},
	{0xd6, 0x07},
	{0xd7, 0x0c},
	{0xd8, 0x01},
	{0xd9, 0x0d},
	{0xe3, 0x80},
	{0xe4, 0x00},
	{0xe5, 0x10},
	{0xe6, 0x00},
	{0xf9, 0x00},
	{0xfa, 0x08},
	{0xfb, 0x00},
};

haptic_bin_file_reg_format_t sih6889_0815_flexible_reg_conts[] = {
	{0x80, 0xe0},
	{0x81, 0x01},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x00},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0x8c, 0xae},
	{0x8d, 0x02},
	{0x8e, 0x00},	
	{0x98, 0x33},
	{0x99, 0x0b},
	{0x9a, 0x00},
	{0xad, 0xff},
	{0xae, 0x0e},
	{0xaf, 0x00},
	{0xc0, 0x00},
	{0xc3, 0xcc},
	{0xc4, 0x04},
	{0xc5, 0x00},
	{0xe0, 0xad},
	{0xe1, 0x01},
	{0xe2, 0x00},
};

haptic_reg_config_group_t sih6889_0815_dedault_reg_config_list = {
	.common = {
		.reg_nums = ARRAY_LEN(sih6889_common_reg_conts),
		.reg_conts = sih6889_common_reg_conts
	},
	.flexible = {
		.reg_nums = ARRAY_LEN(sih6889_0815_flexible_reg_conts),
		.reg_conts = sih6889_0815_flexible_reg_conts
	},
	.lra = {
		.reg_nums = ARRAY_LEN(sih6889_0815_lra_reg_conts),
		.reg_conts = sih6889_0815_lra_reg_conts
	}
};

lra_reg_config_s_t sih6889_config_lists[] = {
	{
		.lra_name = "0809",
		.reg_config_list = &sih6889_0809_dedault_reg_config_list,
	},
	{
		.lra_name = "0815",
		.reg_config_list = &sih6889_0815_dedault_reg_config_list,
	},
	{
		.lra_name = "9595",
		.reg_config_list = &sih6889_9595_dedault_reg_config_list,
	},
};

lra_reg_func_t sih6889_func_list[] = {
	{
		.lra_name = "0809",
		.reg_cont_list = sih6889_lra_0809_config_list,
		.reg_rl_list = sih6889_detect_rl_config_list,
		.reg_vbat_list = sih6889_detect_vbat_config_list,
	},
	{
		.lra_name = "0815",
		.reg_cont_list = sih6889_lra_0815_config_list,
		.reg_rl_list = sih6889_detect_rl_config_list,
		.reg_vbat_list = sih6889_detect_vbat_config_list,
	},
	{
		.lra_name = "9595",
		.reg_cont_list = sih6889_lra_9595_config_list,
		.reg_rl_list = sih6889_detect_rl_config_list,
		.reg_vbat_list = sih6889_detect_vbat_config_list,
	},
};

/*sih6889_load_reg,sih6889_load_func_config 用于开机后做f0校准、电压、电阻检测*/
void sih6889_load_reg(sih_haptic_t *sih_haptic, reg_format_t *reg_list)
{
	uint8_t i;

	for (i = 0; i < HAPTIC_CONFIG_MAX_REG_NUM; i++) {
		if (reg_list[i].operation == OPERATION_END)
			break;
		switch (reg_list[i].operation) {
		case OPERATION_WRITE:
			haptic_regmap_write(sih_haptic->regmapp.regmapping,
				reg_list[i].addr, SIH_I2C_OPERA_BYTE_ONE, &reg_list[i].val);
			hp_info("%s:write 0x%02x:0x%02x\n", __func__,
				reg_list[i].addr, reg_list[i].val);
			break;
		case OPERATION_READ:
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				reg_list[i].addr, SIH_I2C_OPERA_BYTE_ONE, &reg_list[i].val);
			hp_info("%s:read 0x%02x:0x%02x\n", __func__,
				reg_list[i].addr, reg_list[i].val);
			break;
		case OPERATION_BIT:
			haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
				reg_list[i].addr, reg_list[i].mask, reg_list[i].val);
			hp_info("%s:bit 0x%02x:0x%02x\n", __func__,
				reg_list[i].addr, reg_list[i].val);
			break;
		default:
			hp_err("%s:operation err %d\n", __func__, reg_list[i].operation);
			break;
		}
	}
}

void sih6889_load_func_config(sih_haptic_t *sih_haptic, uint8_t func_type)
{
	uint8_t i;
	reg_format_t *reg_list = NULL;

	for (i = 0; i < sizeof(sih6889_func_list) / sizeof(lra_reg_func_t); i++) {
		if (strncmp(sih6889_func_list[i].lra_name,
			sih_haptic->chip_attr.lra_name, SIH_LRA_NAME_LEN) == 0) {
			/* match reg_list */
			switch (func_type) {
			case REG_FUNC_CONT:
				reg_list = sih6889_func_list[i].reg_cont_list;
				break;
			case REG_FUNC_RL:
				reg_list = sih6889_func_list[i].reg_rl_list;
				break;
			case REG_FUNC_VBAT:
				reg_list = sih6889_func_list[i].reg_vbat_list;
				break;
			default:
				hp_err("%s: no func match\n", __func__);
				break;
			}
			break;
		}
	}

	if (!reg_list) {
		hp_err("%s: no reg list match\n", __func__);
		return;
	}
	
	sih6889_load_reg(sih_haptic, reg_list);
}

void sih6889_save_cont_config(sih_haptic_t *sih_haptic, uint32_t *reg_addr,
	uint32_t *reg_value, uint32_t len)
{
	uint8_t i;
	int index = 0;
	reg_format_t *reg_list = NULL;

	for (i = 0; i < ARRAY_LEN(sih6889_func_list); i++) {
		if (!strncmp(sih6889_func_list[i].lra_name, sih_haptic->chip_attr.lra_name,
			SIH_LRA_NAME_LEN)) {
			reg_list = sih6889_func_list[i].reg_cont_list;
			break;
		}
	}

	if (i > ARRAY_LEN(sih6889_func_list)) {
		hp_err("%s: lra not match\n", __func__);
		return;
	}

	/**
	 * save dts reg values into cont func_list
	 * 0x86 0x87 0x88 0x89 0x8a 0x8b or
	 * 0xc3 0xc4 0xc5
	*/
	for (i = 0; i < len; i++) {
		for (index = 0; index < HAPTIC_CONFIG_MAX_REG_NUM; index++) {
			if (reg_list[index].operation == OPERATION_END)
				break;

			if (reg_list[index].addr == reg_addr[i]) {
				reg_list[index].val = reg_value[i];
				hp_dbg("%s: find reg, addr(0x%02x) val(0x%02x); reg_addr(0x%02x) reg_value(0x%02x)\n",
					__func__, reg_list[index].addr, reg_list[index].val, reg_addr[i], reg_value[i]);
				break;
			}
		}
	}
}

/*sih_reg_config_parse_new, sih6889_config_load 用于开机时初始化寄存器*/
void sih_reg_config_parse_new(sih_haptic_t *sih_haptic, haptic_reg_configs_t *reg_config)
{
	uint8_t reg_addr = 0;
	uint8_t reg_value = 0;
	int i;

	hp_info("%s, reg nums = %d\n", __func__, reg_config->reg_nums);
	for (i = 0; i < reg_config->reg_nums; i++) {
		reg_addr = reg_config->reg_conts[i].reg_addr;
		reg_value = reg_config->reg_conts[i].reg_value;
		i2c_write_bytes(sih_haptic, reg_addr, &reg_value, SIH_I2C_OPERA_BYTE_ONE);
		hp_info("%s: 0x%02x:0x%02x\n", __func__, reg_addr, reg_value);
	}
}

int sih6889_config_load(sih_haptic_t *sih_haptic)
{
	int i;

	for (i = 0; i < sizeof(sih6889_config_lists) / sizeof(lra_reg_config_s_t); i++) {
		if (strncmp(sih6889_config_lists[i].lra_name, sih_haptic->chip_attr.lra_name,
			SIH_LRA_NAME_LEN) == 0) {
			/* load func */
			sih_reg_config_parse_new(sih_haptic, &sih6889_config_lists[i].reg_config_list->common);
			sih_reg_config_parse_new(sih_haptic, &sih6889_config_lists[i].reg_config_list->lra);
			sih_reg_config_parse_new(sih_haptic, &sih6889_config_lists[i].reg_config_list->flexible);
			return 0;
		}
	}
	hp_err("%s: no match lra config\n", __func__);
	return -1;
}