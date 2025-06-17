// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX06Cmipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "IMX06C_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx06cmipiraw_Sensor.h"
#include "imx06c_seamless_switch.h"
#include "platform_common.h"


#define _I2C_BUF_SIZE 4096
kal_uint16 imx06c_i2c_data[_I2C_BUF_SIZE];
unsigned int imx06c_size_to_write;
bool imx06c_is_seamless;

static unsigned int g_platform_id;

#undef VENDOR_EDIT
#define USE_BURST_MODE 1
#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

static kal_uint8 qsc_flag;
static kal_uint8 otp_flag;

#if USE_BURST_MODE
static kal_uint16 imx06c_table_write_cmos_sensor(
		kal_uint16 *para, kal_uint32 len);
#endif
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX06C_SENSOR_ID,

	.checksum_value = 0xaf3e324f,

	.pre = {
		.pclk = 1504000000,
		.linelength = 7520,
		.framelength = 6664,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 1504000000,
		.linelength = 7520,
		.framelength = 6664,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 1504000000,
		.linelength = 7520,
		.framelength = 6664,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 1504000000,
		.linelength = 7520,
		.framelength = 3332,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 2464000000,
		.linelength = 12304,
		.framelength = 1664,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 1200,
	},
	.custom1 = {
		.pclk = 1504000000,
		.linelength = 12212,
		.framelength = 4100,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1371430000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 1504000000,
		.linelength = 12212,
		.framelength = 4100,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1371430000,
		.max_framerate = 300,
	},
	.custom3 = { /* 4:3 isz @30fps */
		.pclk = 1504000000,
		.linelength = 9400,
		.framelength = 5332,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1645714286,
		.max_framerate = 300,
	},
	.custom4 = { /* 4:3 isz + DCG @24fps */
		.pclk = 1504000000,
		.linelength = 18545,
		.framelength = 3378,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1371430000,
		.max_framerate = 240,
	},

	.margin = 64,		/* sensor framelength & shutter margin */
	.min_shutter = 6,	/* min shutter */
	.min_gain = 64,
	.max_gain = 8192,
	.min_gain_iso = 100,
	.exp_step = 4,
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xfffc,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 1,/* 1, support; 0,not support */
	.sensor_mode_num = 9,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* enter custom1 delay frame num */
	.custom2_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom3_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom4_delay_frame = 2,	/* enter custom2 delay frame num */
	.frame_time_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/* .mipi_sensor_type = MIPI_OPHY_NCSI2, */
	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_3_LANE,

	.i2c_addr_table = {0x34, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 1000, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{8192, 6144, 0,   0, 8192, 6144, 4096, 3072,
	  0, 0, 4096, 3072,  0,  0, 4096, 3072}, /* preview */
	{8192, 6144, 0,   0, 8192, 6144, 4096, 3072,
	  0, 0, 4096, 3072,  0,  0, 4096, 3072}, /* capture */
	{8192, 6144, 0,  768, 8192, 4608, 4096, 2304,
	0, 0, 4096, 2304,  0,  0, 4096, 2304}, /* normal video */
	{8192, 6144, 0,  768, 8192, 4608, 4096, 2304,
	0, 0, 4096, 2304,  0,  0, 4096, 2304}, /* hs_video */
	{8192, 6144, 0, 1632, 8192, 2880, 2048, 720,
	384, 0, 1280,  720,  0,  0, 1280,  720}, /* slim video */
	{8192, 6144, 0,  768, 8192, 4608, 4096, 2304,
	0, 0, 4096, 2304,  0,  0, 4096, 2304}, /* custom1 */
	{8192, 6144, 0,  0, 8192, 6144, 4096, 3072,
	0, 0, 4096, 3072,  0,  0, 4096, 3072}, /* custom2 */
	{8192, 6144, 2048,  1536, 4096, 3072, 4096, 3072,
	0, 0, 4096, 3072,  0,  0, 4096, 3072}, /* custom3 */
	{8192, 6144, 2048,  1536, 4096, 3072, 4096, 3072,
	0, 0, 4096, 3072,  0,  0, 4096, 3072}, /* custom4 */
};

 /*VC1 for HDR(DT=0X35), VC2 for PDAF(DT=0X36), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[] = {
	/* Preview mode setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0C00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x300, 0x00, 0x00, 0x0000, 0x0000
	},
	 /*capture setting*/
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0C00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x300, 0x00, 0x00, 0x0000, 0x0000
	},
	/* Normal_Video mode setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0900, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x0240, 0x00, 0x00, 0x0000, 0x0000
	},
	/* 4K_Video mode setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0900, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2B, 0x1000, 0x0240, 0x00, 0x00, 0x0000, 0x0000
	},
	/* Slim_Video mode setting */
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x0500, 0x02D0, 0x00, 0x00, 0x0000, 0x0000,
		0x00, 0x00, 0x0000, 0x0000, 0x00, 0x00, 0x0000, 0x0000
	},
	/* Custom1 (DCG) mode 16:9 setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2c, 0x1000, 0x0900, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x0240, 0x00, 0x00, 0x0000, 0x0000
	},
	/* Custom2 (DCG) mode 4:3 setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2c, 0x1000, 0x0C00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x300, 0x00, 0x00, 0x0000, 0x0000
	},
	/*  Custom4 (ISZ) mode 4:3 setting  */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0C00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x300, 0x00, 0x00, 0x0000, 0x0000
	},
	/* Custom5 (ISZ+DCG) mode 4:3 setting */
	{
		0x03, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2c, 0x1000, 0x0C00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x2B, 0x1000, 0x300, 0x00, 0x00, 0x0000, 0x0000
	},
};


/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_binning = {
	.i4OffsetX = 17,
	.i4OffsetY = 12,
	.i4PitchX  =  8,
	.i4PitchY  = 16,
	.i4PairNum  = 8,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 2,
	.i4PosL = { {20, 13}, {18, 15}, {22, 17}, {24, 19},
		   {20, 21}, {18, 23}, {22, 25}, {24, 27} },
	.i4PosR = { {19, 13}, {17, 15}, {21, 17}, {23, 19},
		   {19, 21}, {17, 23}, {21, 25}, {23, 27} },
	.i4BlockNumX = 496,
	.i4BlockNumY = 186,
	.iMirrorFlip = 3,
	.i4Crop = { {0, 0}, {0, 0}, {0, 200}, {0, 0}, {0, 372},
		    {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0} },
};

static kal_uint16 imx06c_QSC_setting[2304 * 2];
static kal_uint16 imx06c_LRC_setting[384 * 2];

/* TODO: measure the delay */
static struct SEAMLESS_SYS_DELAY seamless_sys_delays[] = {
	{ MSDK_SCENARIO_ID_CAMERA_PREVIEW, MSDK_SCENARIO_ID_CUSTOM4, 1 },
	{ MSDK_SCENARIO_ID_CAMERA_PREVIEW, MSDK_SCENARIO_ID_CUSTOM2, 1 },
	{ MSDK_SCENARIO_ID_CUSTOM4, MSDK_SCENARIO_ID_CAMERA_PREVIEW, 1 },
	{ MSDK_SCENARIO_ID_CUSTOM4, MSDK_SCENARIO_ID_CUSTOM2, 1 },
	{ MSDK_SCENARIO_ID_CUSTOM2, MSDK_SCENARIO_ID_CAMERA_PREVIEW, 1 },
	{ MSDK_SCENARIO_ID_CUSTOM2, MSDK_SCENARIO_ID_CUSTOM4, 1 },
};

static struct IMGSENSOR_I2C_CFG *get_i2c_cfg(void)
{
	return &(((struct IMGSENSOR_SENSOR_INST *)
		  (imgsensor.psensor_func->psensor_inst))->i2c_cfg);
}


static int write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
			     (char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
	/* Add this func to set i2c speed by each sensor */
	return imgsensor_i2c_write(
		get_i2c_cfg(),
		pusendcmd,
		4,
		4,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	imgsensor_i2c_read(
		get_i2c_cfg(),
		pusendcmd,
		2,
		(u8 *)&get_byte,
		1,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	imgsensor_i2c_write(
		get_i2c_cfg(),
		pusendcmd,
		3,
		3,
		imgsensor.i2c_write_id,
		IMGSENSOR_I2C_SPEED);
}

static void imx06c_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(regDa[idx]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void imx06c_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(regDa[idx], regDa[idx + 1]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}

static kal_uint16 read_cmos_eeprom_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA0);
	return get_byte;
}

static void read_sensor_Cali(void)
{
	kal_uint16 idx = 0, addr_qsc = 0xfaf, sensor_lrc = 0x7F00;
	kal_uint16 eeprom_lrc_0 = 0x1620, eeprom_lrc_1 = 0x16E0;
	kal_uint16 sensor_lrc_0 = 0x7510, sensor_lrc_1 = 0x7600;
	kal_uint8 otp_data[9] = {0};
	int i = 0;

	/*read otp data to distinguish module*/
	otp_flag = OTP_QSC_NONE;

	for (i = 0; i < 7; i++)
		otp_data[i] = read_cmos_eeprom_8(0x0001 + i);


	/*Internal Module Type*/
	if ((otp_data[0] == 0xff) &&
		(otp_data[1] == 0x00) &&
		(otp_data[2] == 0x0b) &&
		(otp_data[3] == 0x01)) {
		pr_info("OTP type: Internal Only");
		otp_flag = OTP_QSC_INTERNAL;

		for (idx = 0; idx < 2304; idx++) {
			addr_qsc = 0xfaf + idx;
			sensor_lrc = 0x7F00 + idx;
			imx06c_QSC_setting[2 * idx] = sensor_lrc;
			imx06c_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(addr_qsc);
		}

		for (idx = 0; idx < 192; idx++) {
			imx06c_LRC_setting[2 * idx] = sensor_lrc_0 + idx;
				imx06c_LRC_setting[2 * idx + 1] =
			read_cmos_eeprom_8(eeprom_lrc_0 + idx);
			imx06c_LRC_setting[2 * idx + 192 * 2] =
				sensor_lrc_1 + idx;
			imx06c_LRC_setting[2 * idx + 1 + 192 * 2] =
			read_cmos_eeprom_8(eeprom_lrc_1 + idx);
		}

	} else if ((otp_data[5] == 0x56) && (otp_data[6] == 0x00)) {
		/*Internal Module Type*/
		pr_info("OTP type: Custom Only");
		otp_flag = OTP_QSC_CUSTOM;

		for (idx = 0; idx < 2304; idx++) {
			addr_qsc = 0xc90 + idx;
			sensor_lrc = 0x7F00 + idx;
			imx06c_QSC_setting[2 * idx] = sensor_lrc;
			imx06c_QSC_setting[2 * idx + 1] =
				read_cmos_eeprom_8(addr_qsc);
		}

	} else {
		pr_info("OTP type: No Data, 0x0008 = %d, 0x0009 = %d",
		read_cmos_eeprom_8(0x0008), read_cmos_eeprom_8(0x0009));
	}

}


static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* return;*/ /* for test */

	if (!imx06c_is_seamless) {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0340;
		imx06c_i2c_data[imx06c_size_to_write++] = imgsensor.frame_length >> 8;
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0341;
		imx06c_i2c_data[imx06c_size_to_write++] = imgsensor.frame_length & 0xFF;
	}
}	/*	set_dummy  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
	break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
	break;
	}
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug(
		"framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	/*Yijun.Tan@camera.driver,20180116,add for slow shutter */
	int longexposure_times = 0;
	static int long_exposure_status;


	spin_lock(&imgsensor_drv_lock);
	// if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		// imgsensor.frame_length = shutter + imgsensor_info.margin;
	// else
	imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	}

	if (!imx06c_is_seamless)
		write_cmos_sensor_8(0x0104, 0x01);

	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}

	if (!imx06c_is_seamless)
		if (read_cmos_sensor_8(0x0350) != 0x01) {
			pr_debug("single cam scenario enable auto-extend");
			write_cmos_sensor_8(0x0350, 0x01);
		}

	if (longexposure_times > 0) {
		pr_debug("enter long exposure mode, time is %d",
			longexposure_times);
		long_exposure_status = 1;
		// imgsensor.frame_length = shutter + 32;
		if (!imx06c_is_seamless)
			write_cmos_sensor_8(0x3100, longexposure_times & 0x07);
		else {
			imx06c_i2c_data[imx06c_size_to_write++] = 0x3100;
			imx06c_i2c_data[imx06c_size_to_write++] = longexposure_times & 0x07;
		}
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		if (!imx06c_is_seamless)
			write_cmos_sensor_8(0x3100, 0x00);
		else {
			imx06c_i2c_data[imx06c_size_to_write++] = 0x3100;
			imx06c_i2c_data[imx06c_size_to_write++] = 0;
		}

		pr_debug("exit long exposure mode");
	}
	/* Update Shutter */
	if (!imx06c_is_seamless) {
		write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
		write_cmos_sensor_8(0x0203, shutter  & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0202;
		imx06c_i2c_data[imx06c_size_to_write++] = (shutter >> 8) & 0xFF;
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0203;
		imx06c_i2c_data[imx06c_size_to_write++] = shutter & 0xFF;
	}

	pr_debug("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}	/*	write_shutter  */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*0x3500, 0x3501, 0x3502 will increase VBLANK to
	 *get exposure larger than frame exposure
	 *AE doesn't update sensor gain at capture mode,
	 *thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/*if shutter bigger than frame_length,
	 *should extend frame length first
	 */
	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	// if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		// imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340,
					imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
					imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	if (auto_extend_en)
		write_cmos_sensor_8(0x0350, 0x01); /* Enable auto extend */
	else
		write_cmos_sensor_8(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(0x0350));

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = (16384 - (16384 * BASEGAIN) / gain);
	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain = 128 * BASEGAIN;

	if (gain < BASEGAIN || gain > max_gain) {
		pr_debug("Error max gain setting: %d\n", max_gain);

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ",
		gain, reg_gain, max_gain);

	if (!imx06c_is_seamless) {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
		write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	} else {
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0204;
		imx06c_i2c_data[imx06c_size_to_write++] =  (reg_gain>>8) & 0xFF;
		imx06c_i2c_data[imx06c_size_to_write++] = 0x0205;
		imx06c_i2c_data[imx06c_size_to_write++] = reg_gain & 0xFF;
	}

	return gain;
} /* set_gain */

static kal_uint32 imx06c_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	/*
	 * UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;
	 *
	 * grgain_32 = (pSetSensorAWB->ABS_GAIN_GR + 1) >> 1;
	 * rgain_32 = (pSetSensorAWB->ABS_GAIN_R + 1) >> 1;
	 * bgain_32 = (pSetSensorAWB->ABS_GAIN_B + 1) >> 1;
	 * gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB + 1) >> 1;
	 * pr_debug("[%s] ABS_GAIN_GR:%d, grgain_32:%d\n",
	 *	__func__,
	 *	pSetSensorAWB->ABS_GAIN_GR, grgain_32);
	 * pr_debug("[%s] ABS_GAIN_R:%d, rgain_32:%d\n",
	 *	__func__,
	 *	pSetSensorAWB->ABS_GAIN_R, rgain_32);
	 * pr_debug("[%s] ABS_GAIN_B:%d, bgain_32:%d\n",
	 *	__func__,
	 *	pSetSensorAWB->ABS_GAIN_B, bgain_32);
	 * pr_debug("[%s] ABS_GAIN_GB:%d, gbgain_32:%d\n",
	 *	__func__,
	 *	pSetSensorAWB->ABS_GAIN_GB, gbgain_32);
	 *
	 * write_cmos_sensor_8(0x0b8e, (grgain_32 >> 8) & 0xFF);
	 * write_cmos_sensor_8(0x0b8f, grgain_32 & 0xFF);
	 * write_cmos_sensor_8(0x0b90, (rgain_32 >> 8) & 0xFF);
	 * write_cmos_sensor_8(0x0b91, rgain_32 & 0xFF);
	 * write_cmos_sensor_8(0x0b92, (bgain_32 >> 8) & 0xFF);
	 * write_cmos_sensor_8(0x0b93, bgain_32 & 0xFF);
	 * write_cmos_sensor_8(0x0b94, (gbgain_32 >> 8) & 0xFF);
	 * write_cmos_sensor_8(0x0b95, gbgain_32 & 0xFF);
	 *
	 * imx06c_awb_gain_table[1]  = (grgain_32 >> 8) & 0xFF;
	 * imx06c_awb_gain_table[3]  = grgain_32 & 0xFF;
	 * imx06c_awb_gain_table[5]  = (rgain_32 >> 8) & 0xFF;
	 * imx06c_awb_gain_table[7]  = rgain_32 & 0xFF;
	 * imx06c_awb_gain_table[9]  = (bgain_32 >> 8) & 0xFF;
	 * imx06c_awb_gain_table[11] = bgain_32 & 0xFF;
	 * imx06c_awb_gain_table[13] = (gbgain_32 >> 8) & 0xFF;
	 * imx06c_awb_gain_table[15] = gbgain_32 & 0xFF;
	 * imx06c_table_write_cmos_sensor(imx06c_awb_gain_table,
	 *	sizeof(imx06c_awb_gain_table)/sizeof(kal_uint16));
	 */

	return ERROR_NONE;
}

static kal_uint16 imx06c_feedback_awbgain[] = {
	0x0b90, 0x00,
	0x0b91, 0x01,
	0x0b92, 0x00,
	0x0b93, 0x01,
};
/*write AWB gain to sensor*/
static void feedback_awbgain(kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;

	imx06c_feedback_awbgain[1] = r_gain_int;
	imx06c_feedback_awbgain[3] = (
		((r_gain*100) / 512) - (r_gain_int * 100)) * 2;
	imx06c_feedback_awbgain[5] = b_gain_int;
	imx06c_feedback_awbgain[7] = (
		((b_gain * 100) / 512) - (b_gain_int * 100)) * 2;
	imx06c_table_write_cmos_sensor(imx06c_feedback_awbgain,
		sizeof(imx06c_feedback_awbgain)/sizeof(kal_uint16));

}

static void imx06c_set_lsc_reg_setting(
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	pr_debug("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/
	write_cmos_sensor_8(0x9014, 0x01);
	write_cmos_sensor_8(0x4439, 0x01);
	mdelay(1);
	pr_debug("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(0xB870), read_cmos_sensor_8(0x380D));
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(0x9750, 0x01);
	write_cmos_sensor_8(0x9751, 0x01);
	write_cmos_sensor_8(0x9752, 0x01);
	write_cmos_sensor_8(0x9753, 0x01);

	for (i = 0; i < regNum; i++)
		write_cmos_sensor(startAddr[index] + 2*i, regDa[i]);

	write_cmos_sensor_8(0x0B00, 0x00); /*lsc disable*/
}

static void check_stream_is_on(void)
{
	int i = 0;
	UINT32 framecnt;
	// int try_time = 3;
	int timeout = (10000/imgsensor.current_fps)+1;

	for (i = 0; i < timeout; i++) {

		framecnt = read_cmos_sensor_8(0x0005);
		if (framecnt != 0xFF) {
			pr_debug("IMX06C stream is on, %d\n", framecnt);
			// try_time--;
			// if(try_time == 0) {
			//     break;
			// }
			// mdelay(50);
		} else {
			pr_debug("IMX06C stream is not on %d\n", framecnt);
		}
		mdelay(1);
	}
}

/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable) {
		if (read_cmos_sensor_8(0x0350) != 0x01) {
			pr_info("single cam scenario enable auto-extend");
			write_cmos_sensor_8(0x0350, 0x01);
		}
		write_cmos_sensor_8(0x3020, 0x00);/*Mode transition mode change*/
		//write_cmos_sensor_8(0x3021, 0x01);/*complete mode*/
		write_cmos_sensor_8(0x0100, 0X01);
		check_stream_is_on();
	} else
		write_cmos_sensor_8(0x0100, 0x00);
	return ERROR_NONE;
}

#if USE_BURST_MODE
#define MULTI_WRITE 1

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */
static kal_uint16 imx06c_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			imgsensor_i2c_write(
						get_i2c_cfg(),
						puSendCmd,
						tosend,
						3,
						imgsensor.i2c_write_id,
						imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}
	return 0;
}
#endif

static kal_uint16 imx06c_init_setting[] = {
	/* External Clock Setting */
	0x0136, 0x18,
	0x0137, 0x00,
	/* Global Setting */
	0x4513, 0x01,
	0xF800, 0x26,
	0xF801, 0x26,
	0xF802, 0xCC,
	0xF803, 0xE9,
	0xF804, 0x55,
	0xF805, 0xEC,
	0xF806, 0xC7,
	0xF807, 0x00,
	0xF808, 0x19,
	0xF809, 0xA4,
	0xF80A, 0x12,
	0xF80B, 0x22,
	0xF80C, 0x55,
	0xF80D, 0x14,
	0xF80E, 0x51,
	0xF80F, 0x00,
	0xF810, 0x70,
	0xF811, 0xAF,
	0xF812, 0x17,
	0xF813, 0xFA,
	0xF814, 0x55,
	0xF815, 0x8C,
	0xF816, 0x56,
	0xF817, 0x00,
	0xF818, 0xF8,
	0xF819, 0xA9,
	0xF81A, 0x17,
	0xF81B, 0xFA,
	0xF884, 0x4F,
	0xF885, 0x86,
	0xF886, 0xAC,
	0xF887, 0x14,
	0xF888, 0xD1,
	0xF889, 0x20,
	0xF88A, 0x51,
	0xF88B, 0x37,
	0xF88C, 0x1E,
	0xF88D, 0x12,
	0xF88E, 0x50,
	0xF88F, 0x10,
	0xF890, 0xBF,
	0xF891, 0x0B,
	0xF892, 0xD2,
	0xF893, 0x20,
	0xF894, 0x44,
	0xF895, 0xFB,
	0xF896, 0x1E,
	0xF897, 0x22,
	0xF898, 0xBF,
	0xF899, 0x07,
	0xF89A, 0xD0,
	0xF89B, 0x18,
	0xF89C, 0x08,
	0xF89D, 0x00,
	0xF89E, 0xF6,
	0xF89F, 0x00,
	0xF8A0, 0x0F,
	0xF8A1, 0x2E,
	0xF8A2, 0x07,
	0xF8A3, 0x00,
	0xF8A4, 0x60,
	0xF8A5, 0x10,
	0xF8A6, 0x1E,
	0xF8A7, 0x11,
	0xF8A8, 0xBE,
	0xF8A9, 0x8D,
	0xF8AA, 0xD1,
	0xF8AB, 0x18,
	0xF8AC, 0x4D,
	0xF8AD, 0xD8,
	0xF8AE, 0xF6,
	0xF8AF, 0x11,
	0xF8B0, 0x09,
	0xF8B1, 0x42,
	0xF8B2, 0xBE,
	0xF8B3, 0x88,
	0xF8B4, 0xD1,
	0xF8B5, 0x18,
	0xF8B6, 0x08,
	0xF8B7, 0x00,
	0xF8B8, 0xF6,
	0xF8B9, 0x11,
	0xF8BA, 0x0F,
	0xF8BB, 0x2E,
	0xF8BC, 0x60,
	0xF8BD, 0x21,
	0xF8BE, 0x28,
	0xF8BF, 0x01,
	0xF8C0, 0xA7,
	0xF8C1, 0x00,
	0xF8C2, 0xD1,
	0xF8C3, 0x21,
	0xF8C4, 0x43,
	0xF8C5, 0x6C,
	0xF8C6, 0x1E,
	0xF8C7, 0x11,
	0xF8C8, 0xD2,
	0xF8C9, 0x20,
	0xF8CA, 0x50,
	0xF8CB, 0xE6,
	0xF8CC, 0x1A,
	0xF8CD, 0x22,
	0xF8CE, 0xD3,
	0xF8CF, 0x00,
	0xF8D0, 0xFF,
	0xF8D1, 0xFC,
	0xF8D2, 0x24,
	0xF8D3, 0x32,
	0xF8D4, 0xA6,
	0xF8D5, 0x12,
	0xF8D6, 0xA7,
	0xF8D7, 0x02,
	0xF8D8, 0xD4,
	0xF8D9, 0x20,
	0xF8DA, 0x50,
	0xF8DB, 0xE4,
	0xF8DC, 0x1A,
	0xF8DD, 0x44,
	0xF8DE, 0x24,
	0xF8DF, 0x34,
	0xF8E0, 0xA6,
	0xF8E1, 0x14,
	0xF8E2, 0xA7,
	0xF8E3, 0x04,
	0xF8E4, 0x04,
	0xF8E5, 0x24,
	0xF8E6, 0xD2,
	0xF8E7, 0x20,
	0xF8E8, 0x50,
	0xF8E9, 0xB2,
	0xF8EA, 0x1A,
	0xF8EB, 0x22,
	0xF8EC, 0x24,
	0xF8ED, 0x32,
	0xF8EE, 0xA6,
	0xF8EF, 0x12,
	0xF8F0, 0xA7,
	0xF8F1, 0x02,
	0xF8F2, 0x04,
	0xF8F3, 0x42,
	0xF8F4, 0xD4,
	0xF8F5, 0x20,
	0xF8F6, 0x50,
	0xF8F7, 0xBA,
	0xF8F8, 0x1A,
	0xF8F9, 0x44,
	0xF8FA, 0x24,
	0xF8FB, 0x34,
	0xF8FC, 0xA6,
	0xF8FD, 0x13,
	0xF8FE, 0xA7,
	0xF8FF, 0x03,
	0xF900, 0x04,
	0xF901, 0x23,
	0xF902, 0x29,
	0xF903, 0x00,
	0xF904, 0xA6,
	0xF905, 0x10,
	0xF906, 0xA7,
	0xF907, 0x00,
	0xF908, 0x60,
	0xF909, 0x11,
	0xF90A, 0x28,
	0xF90B, 0x01,
	0xF90C, 0xA6,
	0xF90D, 0x11,
	0xF90E, 0xA7,
	0xF90F, 0x01,
	0xF910, 0x04,
	0xF911, 0x01,
	0xF912, 0x04,
	0xF913, 0x31,
	0xF914, 0xD0,
	0xF915, 0x20,
	0xF916, 0x51,
	0xF917, 0x00,
	0xF918, 0x1F,
	0xF919, 0x01,
	0xF91A, 0xA8,
	0xF91B, 0x14,
	0xF91C, 0xA0,
	0xF91D, 0x05,
	0xF91E, 0x00,
	0xF91F, 0x00,
	0x4331, 0x01,

	/* Phy_VIF Setting */
	0x3304, 0x00,

	/* Register Version*/
	0x33F0, 0x02,
	0x33F1, 0x06,

	/* Signaling Mode Setting */
	0x0111, 0x03,

	/* DS CRC Setting */
	0xA5D9, 0x18,
	0xA5DB, 0x18,
	0xA5DF, 0x18,
	0xA5E1, 0x18,
	0xA5F1, 0x10,
	0xA5F3, 0x10,
	0xA5F7, 0x10,
	0xA5F9, 0x10,
	0xA732, 0x00,
	0xA733, 0x00,
	0xA744, 0x00,
	0xA745, 0x00,

	/* MIPI Global Timing Control Setting */
	0x0808, 0x02,

	/* Global Setting */
	0x0902, 0x02,
	0x1600, 0x02,
	0x1601, 0x02,
	0x1602, 0x02,
	0x318B, 0x40,
	0x318C, 0x08,
	0x3B28, 0x00,
	0x3B29, 0x80,
	0x3BC0, 0xBF,
	0x3BC4, 0xBF,
	0x3BC5, 0xBF,
	0x3BC8, 0xBF,
	0x3BCC, 0xBF,
	0x3BCD, 0xBF,
	0x3D00, 0xBF,
	0x3D01, 0xBF,
	0x3D02, 0xBF,
	0x3D03, 0xBF,
	0x3D04, 0xBF,
	0x3D05, 0xBF,
	0x3D06, 0xBF,
	0x3D07, 0xBF,
	0x3D08, 0xBF,
	0x3D09, 0xBF,
	0x3D0A, 0xBF,
	0x3D0B, 0xBF,
	0x3D0C, 0xBF,
	0x3D0D, 0xBF,
	0x3D0E, 0xBF,
	0x3D0F, 0xBF,
	0x3D10, 0xBF,
	0x3D11, 0xBF,
	0x3D12, 0xBF,
	0x3D13, 0xBF,
	0x3D14, 0xBF,
	0x3D15, 0xBF,
	0x3D16, 0xBF,
	0x3D17, 0xBF,
	0x3D18, 0xBF,
	0x3D19, 0xBF,
	0x3D1A, 0xBF,
	0x3D1B, 0xBF,
	0x3D1C, 0xBF,
	0x3D1D, 0xBF,
	0x3D1E, 0xBF,
	0x3D1F, 0xBF,
	0x3D20, 0xBF,
	0x3D21, 0xBF,
	0x3D22, 0xBF,
	0x3D23, 0xBF,
	0x3D24, 0xBF,
	0x3D25, 0xBF,
	0x3D26, 0xBF,
	0x3D27, 0xBF,
	0x3D28, 0xBF,
	0x3D29, 0xBF,
	0x3D2A, 0xBF,
	0x3D2B, 0xBF,
	0x3D2C, 0xBF,
	0x3D2D, 0xBF,
	0x3D2E, 0xBF,
	0x3D2F, 0xBF,
	0x3D30, 0xBF,
	0x3D31, 0xBF,
	0x3D32, 0xBF,
	0x3D33, 0xBF,
	0x3D34, 0xBF,
	0x3D35, 0xBF,
	0x3D36, 0xBF,
	0x3D37, 0xBF,
	0x3D38, 0xBF,
	0x3D39, 0xBF,
	0x3D3A, 0xBF,
	0x3D3B, 0xBF,
	0x3D3C, 0xBF,
	0x3D3D, 0xBF,
	0x3D3E, 0xBF,
	0x3D3F, 0xBF,
	0x3D40, 0xBF,
	0x3D41, 0xBF,
	0x3D42, 0xBF,
	0x3D43, 0xBF,
	0x3D44, 0xBF,
	0x3D45, 0xBF,
	0x3D46, 0xBF,
	0x3D47, 0xBF,
	0x3D48, 0xBF,
	0x3D49, 0xBF,
	0x3D4A, 0xBF,
	0x3D4B, 0xBF,
	0x3D4C, 0xBF,
	0x3D4D, 0xBF,
	0x3D4E, 0xBF,
	0x3D4F, 0xBF,
	0x5081, 0x04,
	0x50C3, 0x70,
	0x55E5, 0x00,
	0x55E6, 0x79,
	0x55E7, 0x18,
	0x6BEE, 0x01,
	0x6BEF, 0xEE,
	0x6BF0, 0x12,
	0x73C5, 0xE9,
	0x73C9, 0xE9,
	0x73CD, 0xE9,
	0x73D1, 0xE9,
	0x742F, 0xE9,
	0x7433, 0xE9,
	0x7437, 0xE9,
	0x743B, 0xE9,
	0x75E3, 0xE9,
	0x75E7, 0xE9,
	0x75EB, 0xE9,
	0x75EF, 0xE9,
	0x93A9, 0xD7,
	0x93AB, 0xD2,
	0x93AD, 0x5A,
	0x93AF, 0x55,
	0x9623, 0x04,
	0x9624, 0x64,
	0x96D7, 0xCC,
	0x96D8, 0xCC,
	0x96D9, 0xCC,
	0x972D, 0x64,
	0x972F, 0x60,
	0x973D, 0x00,
	0x973E, 0x00,
	0x973F, 0x00,
	0x9741, 0x00,
	0x9742, 0x00,
	0x9743, 0x00,
	0x9745, 0x00,
	0x9746, 0x00,
	0x9747, 0x00,
	0x9749, 0x00,
	0x974A, 0x2A,
	0x974B, 0x90,
	0x974D, 0x00,
	0x974E, 0x45,
	0x974F, 0xC0,
	0x9751, 0x00,
	0x9752, 0x28,
	0x9753, 0xA0,
	0x9755, 0x00,
	0x9756, 0x28,
	0x9757, 0xA0,
	0x9759, 0x00,
	0x975A, 0x28,
	0x975B, 0xA0,
	0x975D, 0x00,
	0x975E, 0x55,
	0x975F, 0x80,
	0x9761, 0x00,
	0x9762, 0x70,
	0x9763, 0xB0,
	0x9765, 0x00,
	0x9766, 0x73,
	0x9767, 0x80,
	0x9769, 0x00,
	0x976A, 0x28,
	0x976B, 0xA0,
	0x976D, 0x00,
	0x976E, 0x72,
	0x976F, 0x50,
	0x9771, 0x00,
	0x9772, 0x46,
	0x9775, 0x00,
	0x9776, 0x00,
	0x9777, 0x00,
	0x9779, 0x00,
	0x977A, 0x00,
	0x977B, 0x00,
	0x977D, 0x00,
	0x977E, 0x00,
	0x977F, 0x00,
	0x9781, 0x00,
	0x9782, 0x2D,
	0x9783, 0x40,
	0x9785, 0x00,
	0x9786, 0x48,
	0x9787, 0x70,
	0x9789, 0x00,
	0x978A, 0x00,
	0x978B, 0x00,
	0x978D, 0x00,
	0x978E, 0x00,
	0x978F, 0x00,
	0x9791, 0x00,
	0x9792, 0x00,
	0x9793, 0x00,
	0x9795, 0x00,
	0x9796, 0x34,
	0x9797, 0xD0,
	0x9799, 0x00,
	0x979A, 0x27,
	0x979B, 0x70,
	0x979D, 0x00,
	0x979E, 0x00,
	0x979F, 0x00,
	0x97A1, 0x00,
	0x97A2, 0x00,
	0x97A3, 0x00,
	0x9DCE, 0x09,
	0xA003, 0xBA,
	0xA006, 0x04,
	0xA01E, 0x07,
	0xA01F, 0x08,
	0xA049, 0xCC,
	0xA30B, 0x06,
	0xA311, 0x06,
	0xA317, 0x50,
	0xA31D, 0x50,
	0xA329, 0x06,
	0xA32F, 0x06,
	0xA335, 0x50,
	0xA33B, 0x50,
	0xA573, 0x18,
	0xA575, 0x18,
	0xA579, 0x18,
	0xA57B, 0x18,
	0xA58B, 0x10,
	0xA58D, 0x10,
	0xA591, 0x10,
	0xA593, 0x10,
	0xA730, 0x00,
	0xA738, 0x10,
	0xA742, 0x00,
	0xA74A, 0x10,
	0xA759, 0x00,
	0xA75F, 0x00,
	0xA769, 0x04,
	0xA781, 0x04,
	0xAC1F, 0x3C,
	0xAC21, 0x3C,
	0xAC23, 0x3C,
	0xAE9F, 0x00,
	0xAEA1, 0x00,
	0xAEAB, 0x00,
	0xAEAD, 0x00,
	0xAEB7, 0x00,
	0xAEB9, 0x00,
	0xAECE, 0x04,
	0xAECF, 0x00,
	0xAED0, 0x04,
	0xAED1, 0x00,
	0xAEDA, 0x04,
	0xAEDB, 0x00,
	0xAEDC, 0x04,
	0xAEDD, 0x00,
	0xAEE6, 0x04,
	0xAEE7, 0x00,
	0xAEE8, 0x04,
	0xAEE9, 0x00,
	0xD00E, 0xDF,

	/* Global Setting 2 */
	0x4333, 0x01,
	0x95E6, 0xFF,
	0x95E7, 0xFF,
	0x95EE, 0x00,
	0x95EF, 0x07,
	0x95F0, 0x00,
	0x95F1, 0x9C,
	0x95F8, 0xFF,
	0x95F9, 0xFF,
	0x95FE, 0xFF,
	0x95FF, 0xFF,
	0x9606, 0x00,
	0x9607, 0x07,
	0x9608, 0x00,
	0x9609, 0x9C,
	0x9610, 0xFF,
	0x9611, 0xFF,
	0x9616, 0xFF,
	0x9617, 0xFF,
	0x9628, 0xFF,
	0x9629, 0xFF,
	0x962E, 0xFF,
	0x962F, 0xFF,
	0x9640, 0xFF,
	0x9641, 0xFF,
	0x9646, 0xFF,
	0x9647, 0xFF,
	0x964E, 0x00,
	0x964F, 0x07,
	0x9650, 0x00,
	0x9651, 0x9C,
	0x9658, 0xFF,
	0x9659, 0xFF,
	0x965E, 0xFF,
	0x965F, 0xFF,
	0x9666, 0x00,
	0x9667, 0x07,
	0x9668, 0x00,
	0x9669, 0x9C,
	0x9670, 0xFF,
	0x9671, 0xFF,
	0x9676, 0xFF,
	0x9677, 0xFF,
	0x9688, 0xFF,
	0x9689, 0xFF,
	0x968E, 0xFF,
	0x968F, 0xFF,
	0x96A0, 0xFF,
	0x96A1, 0xFF,
	0x96A6, 0xFF,
	0x96A7, 0xFF,
	0x96B8, 0xFF,
	0x96B9, 0xFF,
	0x96BE, 0xFF,
	0x96BF, 0xFF,
	0x96C6, 0x00,
	0x96C7, 0x07,
	0x96C8, 0x00,
	0x96C9, 0x9C,
	0x96D0, 0xFF,
	0x96D1, 0xFF,
	0x96D6, 0xFF,
	0x96D7, 0xFF,
	0x96E8, 0xFF,
	0x96E9, 0xFF,
	0x96EE, 0xFF,
	0x96EF, 0xFF,
	0x9700, 0xFF,
	0x9701, 0xFF,
	0x9706, 0xFF,
	0x9707, 0xFF,
	0x9718, 0xFF,
	0x9719, 0xFF,
	0x971E, 0xFF,
	0x971F, 0xFF,
	0x9730, 0xFF,
	0x9731, 0xFF,
	0x9736, 0xFF,
	0x9737, 0xFF,
	0x9748, 0xFF,
	0x9749, 0xFF,
	0x974E, 0xFF,
	0x974F, 0xFF,
	0x9760, 0xFF,
	0x9761, 0xFF,
	0x9766, 0xFF,
	0x9767, 0xFF,
	0x976E, 0x00,
	0x976F, 0x07,
	0x9770, 0x00,
	0x9771, 0x9C,
	0x9778, 0xFF,
	0x9779, 0xFF,
	0x977E, 0xFF,
	0x977F, 0xFF,
	0x9786, 0x00,
	0x9787, 0x07,
	0x9788, 0x00,
	0x9789, 0x9C,
	0x9790, 0xFF,
	0x9791, 0xFF,
	0x9796, 0xFF,
	0x9797, 0xFF,
	0x97A8, 0xFF,
	0x97A9, 0xFF,
	0x97AE, 0xFF,
	0x97AF, 0xFF,
	0x97C0, 0xFF,
	0x97C1, 0xFF,
	0x980E, 0x00,
	0x980F, 0x02,
	0x9810, 0x00,
	0x9811, 0x97,
	0x9812, 0x00,
	0x9813, 0x02,
	0x9814, 0x00,
	0x9815, 0x97,
	0x9816, 0x00,
	0x9817, 0x02,
	0x9818, 0x00,
	0x9819, 0x97,
	0x981A, 0x00,
	0x981B, 0x02,
	0x981C, 0x00,
	0x981D, 0x97,
	0x981E, 0x00,
	0x981F, 0x02,
	0x9820, 0x00,
	0x9821, 0x97,
	0x9822, 0x00,
	0x9823, 0x02,
	0x9824, 0x00,
	0x9825, 0x97,
	0x9826, 0x00,
	0x9827, 0x02,
	0x9828, 0x00,
	0x9829, 0x97,
	0x982A, 0x00,
	0x982B, 0x02,
	0x982C, 0x00,
	0x982D, 0x97,
	0x982E, 0x00,
	0x982F, 0x02,
	0x9830, 0x00,
	0x9831, 0x97,
	0x9836, 0x00,
	0x9837, 0x03,
	0x9838, 0x00,
	0x9839, 0x98,
	0x983A, 0x00,
	0x983B, 0x03,
	0x983C, 0x00,
	0x983D, 0x98,
	0x983E, 0x00,
	0x983F, 0x03,
	0x9840, 0x00,
	0x9841, 0x98,
	0x9842, 0x00,
	0x9843, 0x03,
	0x9844, 0x00,
	0x9845, 0x98,
	0x9880, 0xFF,
	0x9881, 0xFF,
	0x9888, 0x00,
	0x9889, 0x07,
	0x988A, 0x00,
	0x988B, 0x9C,
	0x9892, 0xFF,
	0x9893, 0xFF,
	0x9898, 0xFF,
	0x9899, 0xFF,
	0x98A0, 0x00,
	0x98A1, 0x07,
	0x98A2, 0x00,
	0x98A3, 0x9C,
	0x98AA, 0xFF,
	0x98AB, 0xFF,
	0x98B0, 0xFF,
	0x98B1, 0xFF,
	0x98C2, 0xFF,
	0x98C3, 0xFF,
	0x98C8, 0xFF,
	0x98C9, 0xFF,
	0x98DA, 0xFF,
	0x98DB, 0xFF,
	0x98E0, 0xFF,
	0x98E1, 0xFF,
	0x98E8, 0x00,
	0x98E9, 0x07,
	0x98EA, 0x00,
	0x98EB, 0x9C,
	0x98F2, 0xFF,
	0x98F3, 0xFF,
	0x98F8, 0xFF,
	0x98F9, 0xFF,
	0x9900, 0x00,
	0x9901, 0x07,
	0x9902, 0x00,
	0x9903, 0x9C,
	0x990A, 0xFF,
	0x990B, 0xFF,
	0x9910, 0xFF,
	0x9911, 0xFF,
	0x9922, 0xFF,
	0x9923, 0xFF,
	0x9928, 0xFF,
	0x9929, 0xFF,
	0x993A, 0xFF,
	0x993B, 0xFF,
	0x9940, 0xFF,
	0x9941, 0xFF,
	0x9952, 0xFF,
	0x9953, 0xFF,
	0x9958, 0xFF,
	0x9959, 0xFF,
	0x9960, 0x00,
	0x9961, 0x07,
	0x9962, 0x00,
	0x9963, 0x9C,
	0x996A, 0xFF,
	0x996B, 0xFF,
	0x9970, 0xFF,
	0x9971, 0xFF,
	0x9982, 0xFF,
	0x9983, 0xFF,
	0x9988, 0xFF,
	0x9989, 0xFF,
	0x999A, 0xFF,
	0x999B, 0xFF,
	0x99A0, 0xFF,
	0x99A1, 0xFF,
	0x99B2, 0xFF,
	0x99B3, 0xFF,
	0x99B8, 0xFF,
	0x99B9, 0xFF,
	0x99CA, 0xFF,
	0x99CB, 0xFF,
	0x99D0, 0xFF,
	0x99D1, 0xFF,
	0x99E2, 0xFF,
	0x99E3, 0xFF,
	0x99E8, 0xFF,
	0x99E9, 0xFF,
	0x99FA, 0xFF,
	0x99FB, 0xFF,
	0x9A00, 0xFF,
	0x9A01, 0xFF,
	0x9A08, 0x00,
	0x9A09, 0x07,
	0x9A0A, 0x00,
	0x9A0B, 0x9C,
	0x9A12, 0xFF,
	0x9A13, 0xFF,
	0x9A18, 0xFF,
	0x9A19, 0xFF,
	0x9A20, 0x00,
	0x9A21, 0x07,
	0x9A22, 0x00,
	0x9A23, 0x9C,
	0x9A2A, 0xFF,
	0x9A2B, 0xFF,
	0x9A30, 0xFF,
	0x9A31, 0xFF,
	0x9A42, 0xFF,
	0x9A43, 0xFF,
	0x9A48, 0xFF,
	0x9A49, 0xFF,
	0x9A5A, 0xFF,
	0x9A5B, 0xFF,
	0x9AA8, 0x00,
	0x9AA9, 0x02,
	0x9AAA, 0x00,
	0x9AAB, 0x97,
	0x9AAC, 0x00,
	0x9AAD, 0x02,
	0x9AAE, 0x00,
	0x9AAF, 0x97,
	0x9AB0, 0x00,
	0x9AB1, 0x02,
	0x9AB2, 0x00,
	0x9AB3, 0x97,
	0x9AB4, 0x00,
	0x9AB5, 0x02,
	0x9AB6, 0x00,
	0x9AB7, 0x97,
	0x9AB8, 0x00,
	0x9AB9, 0x02,
	0x9ABA, 0x00,
	0x9ABB, 0x97,
	0x9ABC, 0x00,
	0x9ABD, 0x02,
	0x9ABE, 0x00,
	0x9ABF, 0x97,
	0x9AC0, 0x00,
	0x9AC1, 0x02,
	0x9AC2, 0x00,
	0x9AC3, 0x97,
	0x9AC4, 0x00,
	0x9AC5, 0x02,
	0x9AC6, 0x00,
	0x9AC7, 0x97,
	0x9AC8, 0x00,
	0x9AC9, 0x02,
	0x9ACA, 0x00,
	0x9ACB, 0x97,
	0x9AD0, 0x00,
	0x9AD1, 0x03,
	0x9AD2, 0x00,
	0x9AD3, 0x98,
	0x9AD4, 0x00,
	0x9AD5, 0x03,
	0x9AD6, 0x00,
	0x9AD7, 0x98,
	0x9AD8, 0x00,
	0x9AD9, 0x03,
	0x9ADA, 0x00,
	0x9ADB, 0x98,
	0x9ADC, 0x00,
	0x9ADD, 0x03,
	0x9ADE, 0x00,
	0x9ADF, 0x98,
	0x9CC5, 0x00,
	0x9CD1, 0x00,
	0x9CDD, 0x00,
	0x9EB5, 0xFF,
	0x9EC1, 0xFF,
	0x9F3F, 0xFF,
	0x9F43, 0x00,
	0x9F4B, 0xFF,
	0x9F4F, 0x00,
	0xA281, 0x14,
	0xA287, 0x14,
	0xA2A3, 0x14,
	0xA2A9, 0x14,
	0x944E, 0x14,
	0x9454, 0x14,
	0x4333, 0x00,
	0x4333, 0x01,
	0xB440, 0x01,
	0xB441, 0x01,
	0xB442, 0x01,
	0xB443, 0x01,
	0xB444, 0x01,
	0xB445, 0x01,
	0xB446, 0x01,
	0xB447, 0x01,
	0xB448, 0x01,
	0xB449, 0x01,
	0xB44A, 0x01,
	0xB44B, 0x01,
	0xB44C, 0x01,
	0xB44D, 0x01,
	0xB450, 0x01,
	0xB451, 0x01,
	0xB452, 0x01,
	0xB453, 0x01,
	0xB454, 0x01,
	0xB457, 0x01,
	0xB458, 0x01,
	0xB459, 0x01,
	0xB45A, 0x01,
	0xBE1E, 0x22,
	0xBE1F, 0x26,
	0xBE20, 0x22,
	0xBE21, 0x22,
	0xBE22, 0x22,
	0xBE23, 0x22,
	0xBE24, 0x26,
	0xBE25, 0x26,
	0xBE26, 0x26,
	0xBE27, 0x1E,
	0xBE28, 0x1E,
	0xBE29, 0x1E,
	0xBE2A, 0x1E,
	0xBE2B, 0x26,
	0xBE2C, 0x26,
	0xBE2D, 0x26,
	0xBE2E, 0x26,
	0xBE2F, 0x22,
	0xBE30, 0x26,
	0xBE31, 0x22,
	0xBE32, 0x22,
	0xBE33, 0x22,
	0xBE34, 0x22,
	0xBE35, 0x26,
	0xBE36, 0x26,
	0xBE37, 0x26,
	0xBE38, 0x1E,
	0xBE39, 0x1E,
	0xBE3A, 0x1E,
	0xBE3B, 0x1E,
	0xBE3C, 0x26,
	0xBE3D, 0x26,
	0xBE3E, 0x26,
	0xBE3F, 0x26,
	0xBE40, 0x22,
	0xBE41, 0x26,
	0xBE42, 0x22,
	0xBE43, 0x22,
	0xBE44, 0x22,
	0xBE45, 0x22,
	0xBE46, 0x26,
	0xBE47, 0x26,
	0xBE48, 0x26,
	0xBE49, 0x1E,
	0xBE4A, 0x1E,
	0xBE4B, 0x1E,
	0xBE4C, 0x1E,
	0xBE4D, 0x26,
	0xBE4E, 0x26,
	0xBE4F, 0x26,
	0xBE50, 0x26,
	0xBE51, 0x22,
	0xBE52, 0x26,
	0xBE53, 0x22,
	0xBE54, 0x22,
	0xBE55, 0x22,
	0xBE56, 0x22,
	0xBE57, 0x26,
	0xBE58, 0x26,
	0xBE59, 0x26,
	0xBE5A, 0x1E,
	0xBE5B, 0x1E,
	0xBE5C, 0x1E,
	0xBE5D, 0x1E,
	0xBE5E, 0x26,
	0xBE5F, 0x26,
	0xBE60, 0x26,
	0xBE61, 0x26,
	0xBE62, 0x22,
	0xBE63, 0x26,
	0xBE64, 0x22,
	0xBE65, 0x22,
	0xBE66, 0x22,
	0xBE67, 0x22,
	0xBE68, 0x26,
	0xBE69, 0x26,
	0xBE6A, 0x26,
	0xBE6B, 0x1E,
	0xBE6C, 0x1E,
	0xBE6D, 0x1E,
	0xBE6E, 0x1E,
	0xBE6F, 0x26,
	0xBE70, 0x26,
	0xBE71, 0x26,
	0xBE72, 0x26,
	0xBE73, 0x22,
	0xBE74, 0x26,
	0xBE75, 0x22,
	0xBE76, 0x22,
	0xBE77, 0x22,
	0xBE78, 0x22,
	0xBE79, 0x26,
	0xBE7A, 0x26,
	0xBE7B, 0x26,
	0xBE7C, 0x1E,
	0xBE7D, 0x1E,
	0xBE7E, 0x1E,
	0xBE7F, 0x1E,
	0xBE80, 0x26,
	0xBE81, 0x26,
	0xBE82, 0x26,
	0xBE83, 0x22,
	0xBE84, 0x26,
	0xBE85, 0x22,
	0xBE86, 0x22,
	0xBE87, 0x22,
	0xBE88, 0x22,
	0xBE89, 0x26,
	0xBE8A, 0x26,
	0xBE8B, 0x26,
	0xBE8C, 0x1E,
	0xBE8D, 0x1E,
	0xBE8E, 0x1E,
	0xBE8F, 0x1E,
	0xBE90, 0x26,
	0xBE91, 0x26,
	0xBE92, 0x22,
	0xBE93, 0x22,
	0xBE94, 0x22,
	0xBE95, 0x22,
	0xBE96, 0x26,
	0xBE97, 0x26,
	0xBE98, 0x26,
	0xBE99, 0x1E,
	0xBE9A, 0x1E,
	0xBE9B, 0x1E,
	0xBE9C, 0x1E,
	0xBE9D, 0x26,
	0xBE9E, 0x22,
	0xBE9F, 0x22,
	0xBEA0, 0x22,
	0xBEA1, 0x22,
	0xBEA2, 0x26,
	0xBEA3, 0x26,
	0xBEA4, 0x26,
	0xBEA5, 0x1E,
	0xBEA6, 0x1E,
	0xBEA7, 0x1E,
	0xBEA8, 0x1E,
	0xB47D, 0x20,
	0xB481, 0x2B,
	0xB491, 0x12,
	0xB4A3, 0x4D,
	0xB4A5, 0x15,
	0xB4A7, 0x2F,
	0xB4A9, 0x40,
	0xB4AB, 0x25,
	0xB4AF, 0x2E,
	0xB4B3, 0x16,
	0xB4B5, 0x1B,
	0xB4B7, 0x10,
	0xB4B9, 0x13,
	0xB4BD, 0x11,
	0xB4BF, 0x10,
	0xB4C3, 0x4C,
	0xB4C7, 0x4D,
	0xB4C9, 0x19,
	0xB4CB, 0x17,
	0xB4CD, 0x1A,
	0xB4CF, 0x11,
	0xB4D1, 0x15,
	0xB4D3, 0x2F,
	0xB4D5, 0x40,
	0xB4D7, 0x41,
	0xB4D9, 0x3C,
	0xB4DD, 0x40,
	0xB4E1, 0x1B,
	0xB4E3, 0x1A,
	0xB4E5, 0x14,
	0xB4E7, 0x11,
	0xB4EB, 0x0C,
	0xB4ED, 0x1C,
	0xB4F1, 0x19,
	0xB4F5, 0x15,
	0xB4F7, 0x18,
	0xB4F9, 0x14,
	0xB4FB, 0x12,
	0xB4FD, 0x10,
	0xB4FF, 0x16,
	0xB501, 0x16,
	0xB503, 0x11,
	0xB505, 0x13,
	0xB507, 0x3E,
	0xB50B, 0x42,
	0xB50F, 0x1E,
	0xB511, 0x2C,
	0xB513, 0x20,
	0xB515, 0x28,
	0xB519, 0x21,
	0xB51B, 0x3A,
	0xB51F, 0x2E,
	0xB523, 0x2F,
	0xB525, 0x1A,
	0xB527, 0x21,
	0xB529, 0x21,
	0xB52B, 0x29,
	0xB52D, 0x3A,
	0xB52F, 0x38,
	0xB531, 0x3B,
	0xB533, 0x39,
	0xB535, 0x3F,
	0xB539, 0x42,
	0xB53D, 0x38,
	0xB53F, 0x3C,
	0xB541, 0x3A,
	0xB543, 0x3F,
	0xB547, 0x34,
	0xB549, 0x37,
	0xB54D, 0x37,
	0xB551, 0x40,
	0xB553, 0x35,
	0xB555, 0x34,
	0xB557, 0x35,
	0xB559, 0x3F,
	0xB55B, 0x3A,
	0xB55D, 0x3C,
	0xB55F, 0x41,
	0xB561, 0x40,
	0xB563, 0x3C,
	0xB567, 0x40,
	0xB56B, 0x3A,
	0xB56D, 0x35,
	0xB56F, 0x3D,
	0xB571, 0x41,
	0xB575, 0x37,
	0xB577, 0x3A,
	0xB57B, 0x37,
	0xB57F, 0x41,
	0xB581, 0x37,
	0xB583, 0x38,
	0xB585, 0x37,
	0xB587, 0x42,
	0xB589, 0x37,
	0xB58B, 0x3A,
	0xB58D, 0x41,
	0xB58F, 0x43,
	0xB593, 0x45,
	0xB597, 0x37,
	0xB599, 0x38,
	0xB59B, 0x3C,
	0xB59D, 0x42,
	0xB5A1, 0x3D,
	0xB5A3, 0x38,
	0xB5A7, 0x3B,
	0xB5AB, 0x41,
	0xB5AD, 0x35,
	0xB5AF, 0x39,
	0xB5B1, 0x38,
	0xB5B3, 0x43,
	0xB5B5, 0x3B,
	0xB5B7, 0x3B,
	0xB5B9, 0x39,
	0xB5BB, 0x38,
	0xB5BD, 0x3C,
	0xB5BF, 0x41,
	0xB5C3, 0x44,
	0xB5C5, 0x3A,
	0xB5C9, 0x3A,
	0xB5CD, 0x41,
	0xB5CF, 0x3A,
	0xB5D3, 0x36,
	0xB5D7, 0x40,
	0xB5D9, 0x3C,
	0xB5DB, 0x3F,
	0xB5DD, 0x3C,
	0xB5DF, 0x44,
	0xB5E3, 0x47,
	0xB5E5, 0x3D,
	0xB5E9, 0x3E,
	0xB5ED, 0x41,
	0xB5EF, 0x3E,
	0xB5F3, 0x3D,
	0xB5F6, 0x71,
	0xB5F8, 0x94,
	0xB600, 0x41,
	0xB609, 0x38,
	0xB60A, 0x47,
	0xB60B, 0x99,
	0xB60C, 0x81,
	0xB60D, 0x7F,
	0xB60F, 0x9F,
	0xB611, 0x47,
	0xB612, 0x54,
	0xB613, 0x42,
	0xB614, 0x22,
	0xB616, 0x42,
	0xB617, 0x41,
	0xB619, 0x2F,
	0xB61B, 0x38,
	0xB61C, 0x4E,
	0xB61D, 0x43,
	0xB61E, 0x4F,
	0xB61F, 0x2F,
	0xB620, 0x47,
	0xB621, 0x99,
	0xB622, 0x81,
	0xB623, 0x85,
	0xB624, 0x6B,
	0xB626, 0x82,
	0xB628, 0x53,
	0xB629, 0x5C,
	0xB62A, 0x4D,
	0xB62B, 0x1D,
	0xB62D, 0x41,
	0xB62E, 0x4C,
	0xB630, 0x4A,
	0xB632, 0x47,
	0xB633, 0x4F,
	0xB634, 0x45,
	0xB635, 0x50,
	0xB636, 0x35,
	0xB637, 0x50,
	0xB638, 0x51,
	0xB639, 0x4E,
	0xB63A, 0x51,
	0xB63B, 0x71,
	0xB63D, 0x89,
	0xB63F, 0xA3,
	0xB640, 0xC5,
	0xB641, 0x9D,
	0xB642, 0x39,
	0xB644, 0xA3,
	0xB645, 0x98,
	0xB647, 0x9F,
	0xB649, 0x99,
	0xB64A, 0xA8,
	0xB64B, 0x92,
	0xB64C, 0xA2,
	0xB64D, 0x84,
	0xB64E, 0xA0,
	0xB64F, 0xA0,
	0xB650, 0xA0,
	0xB651, 0x68,
	0xB652, 0x77,
	0xB654, 0x8D,
	0xB656, 0x7C,
	0xB657, 0x81,
	0xB658, 0x7B,
	0xB659, 0x38,
	0xB65B, 0x92,
	0xB65C, 0x83,
	0xB65E, 0x81,
	0xB660, 0x81,
	0xB661, 0x83,
	0xB662, 0x72,
	0xB663, 0x8C,
	0xB664, 0x69,
	0xB665, 0x8C,
	0xB666, 0x90,
	0xB667, 0x90,
	0xB668, 0x90,
	0xB669, 0x8A,
	0xB66B, 0x9F,
	0xB66D, 0x80,
	0xB66E, 0x43,
	0xB66F, 0x83,
	0xB670, 0x31,
	0xB672, 0x90,
	0xB673, 0x8A,
	0xB675, 0x82,
	0xB677, 0x85,
	0xB678, 0x8C,
	0xB679, 0x7F,
	0xB67A, 0x8F,
	0xB67B, 0x6F,
	0xB67C, 0x84,
	0xB67D, 0x8A,
	0xB67E, 0x90,
	0xB67F, 0x90,
	0xB681, 0x9D,
	0xB683, 0x7E,
	0xB684, 0x5E,
	0xB685, 0x7F,
	0xB686, 0x31,
	0xB688, 0x8B,
	0xB689, 0x84,
	0xB68B, 0x83,
	0xB68D, 0x88,
	0xB68E, 0x8D,
	0xB68F, 0x7F,
	0xB690, 0x91,
	0xB691, 0x71,
	0xB692, 0x90,
	0xB693, 0x81,
	0xB694, 0x81,
	0xB695, 0xB0,
	0xB696, 0x89,
	0xB697, 0x1B,
	0xB699, 0xA0,
	0xB69A, 0x8B,
	0xB69C, 0x86,
	0xB69E, 0x8B,
	0xB69F, 0x95,
	0xB6A1, 0x91,
	0xB6A3, 0x90,
	0xB6A4, 0x84,
	0xB6A5, 0x92,
	0xB6A6, 0x8B,
	0xB6A7, 0x28,
	0xB6A9, 0x8C,
	0xB6AA, 0x94,
	0xB6AC, 0x8D,
	0xB6AE, 0x90,
	0xB6AF, 0x94,
	0xB6B1, 0x94,
	0xB6B3, 0x14,
	0xB6B5, 0x14,
	0xB6BD, 0x14,
	0xB6C6, 0x0F,
	0xB6C7, 0x0F,
	0xB6C8, 0x0E,
	0xB6C9, 0x0F,
	0xB6CA, 0x10,
	0xB6CC, 0x0F,
	0xB6CE, 0x10,
	0xB6CF, 0x10,
	0xB6D0, 0x10,
	0xB6D1, 0x10,
	0xB6D3, 0x0D,
	0xB6D4, 0x10,
	0xB6D6, 0x0F,
	0xB6D8, 0x0F,
	0xB6D9, 0x10,
	0xB6DA, 0x10,
	0xB6DB, 0x10,
	0xB6DC, 0x10,
	0xB6DD, 0x0F,
	0xB6DE, 0x0E,
	0xB6DF, 0x0F,
	0xB6E0, 0x0D,
	0xB6E1, 0x11,
	0xB6E3, 0x10,
	0xB6E5, 0x0F,
	0xB6E6, 0x0F,
	0xB6E7, 0x0F,
	0xB6E8, 0x0F,
	0xB6EA, 0x0D,
	0xB6EB, 0x0F,
	0xB6ED, 0x0E,
	0xB6EF, 0x0F,
	0xB6F0, 0x0F,
	0xB6F1, 0x0F,
	0xB6F2, 0x0F,
	0xB6F3, 0x0F,
	0xB6F4, 0x0E,
	0xB6F5, 0x0F,
	0xB6F6, 0x0D,
	0xB6F7, 0x0C,
	0xB6F8, 0x0E,
	0xB6FA, 0x0D,
	0xB6FC, 0x10,
	0xB6FD, 0x10,
	0xB6FE, 0x10,
	0xB6FF, 0x10,
	0xB701, 0x0D,
	0xB702, 0x10,
	0xB704, 0x0F,
	0xB706, 0x0E,
	0xB707, 0x11,
	0xB708, 0x11,
	0xB709, 0x10,
	0xB70A, 0x10,
	0xB70B, 0x0F,
	0xB70C, 0x0D,
	0xB70D, 0x0C,
	0xB70E, 0x0B,
	0xB70F, 0x0C,
	0xB711, 0x0C,
	0xB713, 0x11,
	0xB714, 0x11,
	0xB715, 0x10,
	0xB716, 0x10,
	0xB718, 0x0D,
	0xB719, 0x10,
	0xB71B, 0x10,
	0xB71D, 0x0F,
	0xB71E, 0x11,
	0xB71F, 0x11,
	0xB720, 0x11,
	0xB721, 0x11,
	0xB722, 0x0D,
	0xB723, 0x0C,
	0xB724, 0x0B,
	0xB725, 0x0B,
	0xB726, 0x0A,
	0xB728, 0x0A,
	0xB72A, 0x0E,
	0xB72B, 0x0E,
	0xB72C, 0x0E,
	0xB72D, 0x0E,
	0xB72F, 0x0C,
	0xB730, 0x0D,
	0xB732, 0x0D,
	0xB734, 0x0D,
	0xB735, 0x0E,
	0xB736, 0x0E,
	0xB737, 0x0E,
	0xB738, 0x0E,
	0xB739, 0x0C,
	0xB73A, 0x0B,
	0xB73B, 0x0B,
	0xB73C, 0x0A,
	0xB73E, 0x09,
	0xB740, 0x0C,
	0xB741, 0x0C,
	0xB742, 0x0C,
	0xB743, 0x0C,
	0xB745, 0x0B,
	0xB746, 0x0C,
	0xB748, 0x0C,
	0xB74A, 0x0C,
	0xB74B, 0x0D,
	0xB74C, 0x0D,
	0xB74D, 0x0D,
	0xB74E, 0x0D,
	0xB74F, 0x0B,
	0xB750, 0x0B,
	0xB751, 0x0A,
	0xB752, 0x0A,
	0xB753, 0x0A,
	0xB754, 0x0A,
	0xB756, 0x0A,
	0xB757, 0x09,
	0xB759, 0x09,
	0xB75B, 0x09,
	0xB75C, 0x0A,
	0xB75E, 0x0A,
	0xB760, 0x0B,
	0xB761, 0x0A,
	0xB762, 0x09,
	0xB763, 0x09,
	0xB764, 0x0A,
	0xB766, 0x0A,
	0xB767, 0x09,
	0xB769, 0x09,
	0xB76B, 0x09,
	0xB76C, 0x0A,
	0xB76E, 0x0A,
	0xB770, 0x17,
	0xB772, 0x18,
	0xB785, 0x0F,
	0xB786, 0x11,
	0xB787, 0x09,
	0xB789, 0x0B,
	0xB79B, 0x0F,
	0xB79C, 0x11,
	0xB79D, 0x08,
	0xB79E, 0x0C,
	0xB7A0, 0x0C,
	0xB7B1, 0x0F,
	0xB7B2, 0x11,
	0xB7B3, 0x08,
	0xB7B4, 0x05,
	0xB7B5, 0x07,
	0xB7B7, 0x08,
	0xB7B9, 0x08,
	0xB7BA, 0x0B,
	0xB7BB, 0x0A,
	0xB7BC, 0x09,
	0xB7BF, 0x0B,
	0xB7C1, 0x0D,
	0xB7C3, 0x0F,
	0xB7C4, 0x09,
	0xB7C5, 0x05,
	0xB7C6, 0x0E,
	0xB7C7, 0x09,
	0xB7C8, 0x11,
	0xB7C9, 0x08,
	0xB7CA, 0x05,
	0xB7CB, 0x02,
	0xB7CC, 0x01,
	0xB7CE, 0x03,
	0xB7D0, 0x0A,
	0xB7D1, 0x0B,
	0xB7D2, 0x0E,
	0xB7D3, 0x12,
	0xB7D5, 0x01,
	0xB7D6, 0x0F,
	0xB7D8, 0x0C,
	0xB7DA, 0x11,
	0xB7DB, 0x0D,
	0xB7DC, 0x08,
	0xB7DD, 0x0B,
	0xB7DE, 0x0B,
	0xB7DF, 0x08,
	0xB7E0, 0x05,
	0xB7E1, 0x02,
	0xB7E7, 0x06,
	0xB7E8, 0x05,
	0xB7E9, 0x06,
	0xB7EA, 0x07,
	0xB7ED, 0x07,
	0xB7EF, 0x08,
	0xB7F1, 0x08,
	0xB7F2, 0x06,
	0xB7F3, 0x05,
	0xB7F4, 0x06,
	0xB7F5, 0x06,
	0xB7F6, 0x05,
	0xB7F7, 0x02,
	0xB7FD, 0x03,
	0xB7FE, 0x06,
	0xB7FF, 0x03,
	0xB800, 0x02,
	0xB803, 0x02,
	0xB805, 0x02,
	0xB807, 0x05,
	0xB808, 0x02,
	0xB809, 0x03,
	0xB80A, 0x04,
	0xB80B, 0x04,
	0xB80C, 0x02,
	0xB82D, 0x0D,
	0xB82F, 0x2A,
	0xB837, 0x0D,
	0xB840, 0x0D,
	0xB841, 0x1C,
	0xB842, 0x17,
	0xB843, 0x18,
	0xB844, 0x1D,
	0xB846, 0x2E,
	0xB848, 0x3E,
	0xB849, 0x2E,
	0xB84A, 0x0A,
	0xB84B, 0x22,
	0xB84D, 0x0D,
	0xB84E, 0x09,
	0xB850, 0x25,
	0xB852, 0x0D,
	0xB853, 0x3C,
	0xB854, 0x3E,
	0xB855, 0x3C,
	0xB856, 0x05,
	0xB857, 0x1C,
	0xB858, 0x17,
	0xB859, 0x18,
	0xB85A, 0x19,
	0xB85B, 0x31,
	0xB85D, 0x1C,
	0xB85F, 0x3E,
	0xB860, 0x3C,
	0xB861, 0x35,
	0xB862, 0x1D,
	0xB864, 0x0A,
	0xB865, 0x39,
	0xB867, 0x3A,
	0xB869, 0x1C,
	0xB86A, 0x3E,
	0xB86B, 0x3C,
	0xB86C, 0x3B,
	0xB86D, 0x0E,
	0xB86E, 0x2F,
	0xB86F, 0x1E,
	0xB870, 0x3E,
	0xB871, 0x1E,
	0xB872, 0x2D,
	0xB874, 0x27,
	0xB876, 0x3A,
	0xB877, 0x3B,
	0xB878, 0x34,
	0xB879, 0x39,
	0xB87B, 0x2E,
	0xB87C, 0x1B,
	0xB87E, 0x38,
	0xB880, 0x17,
	0xB881, 0x39,
	0xB882, 0x3D,
	0xB883, 0x36,
	0xB884, 0x30,
	0xB885, 0x18,
	0xB886, 0x23,
	0xB887, 0x14,
	0xB888, 0x0A,
	0xB889, 0x2E,
	0xB88B, 0x28,
	0xB88D, 0x2F,
	0xB88E, 0x35,
	0xB88F, 0x32,
	0xB890, 0x38,
	0xB892, 0x2F,
	0xB893, 0x1E,
	0xB895, 0x2D,
	0xB897, 0x18,
	0xB898, 0x32,
	0xB899, 0x2E,
	0xB89A, 0x3A,
	0xB89B, 0x2C,
	0xB89C, 0x1A,
	0xB89D, 0x28,
	0xB89E, 0x24,
	0xB89F, 0x25,
	0xB8A0, 0x16,
	0xB8A2, 0x23,
	0xB8A4, 0x2C,
	0xB8A5, 0x06,
	0xB8A6, 0x2E,
	0xB8A7, 0x31,
	0xB8A9, 0x27,
	0xB8AA, 0x2A,
	0xB8AC, 0x26,
	0xB8AE, 0x19,
	0xB8AF, 0x2E,
	0xB8B0, 0x30,
	0xB8B1, 0x2E,
	0xB8B2, 0x19,
	0xB8B3, 0x21,
	0xB8B4, 0x22,
	0xB8B5, 0x25,
	0xB8B6, 0x27,
	0xB8B8, 0x14,
	0xB8BA, 0x27,
	0xB8BB, 0x19,
	0xB8BC, 0x1B,
	0xB8BD, 0x31,
	0xB8BF, 0x16,
	0xB8C0, 0x16,
	0xB8C2, 0x26,
	0xB8C4, 0x19,
	0xB8C5, 0x18,
	0xB8C6, 0x2E,
	0xB8C7, 0x2E,
	0xB8C8, 0x28,
	0xB8C9, 0x24,
	0xB8CA, 0x11,
	0xB8CB, 0x11,
	0xB8CC, 0x13,
	0xB8CD, 0x23,
	0xB8CE, 0x1B,
	0xB8D0, 0x18,
	0xB8D1, 0x12,
	0xB8D3, 0x11,
	0xB8D5, 0x17,
	0xB8D6, 0x22,
	0xB8D8, 0x17,
	0xB8DA, 0x23,
	0xB8DB, 0x11,
	0xB8DC, 0x20,
	0xB8DD, 0x13,
	0xB8DE, 0x28,
	0xB8E0, 0x24,
	0xB8E1, 0x22,
	0xB8E3, 0x13,
	0xB8E5, 0x21,
	0xB8E6, 0x22,
	0xB8E8, 0x15,
	0xB8FD, 0x02,
	0xB8FE, 0x03,
	0xB905, 0x02,
	0xB906, 0x04,
	0xB907, 0x03,
	0xB908, 0x03,
	0xB90A, 0x05,
	0xB90B, 0x03,
	0xB90D, 0x02,
	0xB90F, 0x02,
	0xB910, 0x03,
	0xB911, 0x02,
	0xB912, 0x04,
	0xB913, 0x03,
	0xB914, 0x03,
	0xB91C, 0x04,
	0xB91D, 0x03,
	0xB91E, 0x04,
	0xB91F, 0x03,
	0xB921, 0x07,
	0xB922, 0x04,
	0xB924, 0x05,
	0xB926, 0x03,
	0xB927, 0x03,
	0xB928, 0x05,
	0xB929, 0x02,
	0xB92A, 0x04,
	0xB934, 0x04,
	0xB940, 0x04,
	0xB95D, 0x05,
	0xB95F, 0x04,
	0xB969, 0x01,
	0xB973, 0x06,
	0xB975, 0x05,
	0xB978, 0x02,
	0xB979, 0x01,
	0xB97A, 0x01,
	0xB97C, 0x01,
	0xB988, 0x04,
	0xB989, 0x04,
	0xB98A, 0x04,
	0xB98B, 0x04,
	0xB98D, 0x07,
	0xB98E, 0x06,
	0xB990, 0x06,
	0xB992, 0x04,
	0xB993, 0x05,
	0xB995, 0x05,
	0xB998, 0x05,
	0xB999, 0x05,
	0xB99A, 0x05,
	0xB99B, 0x06,
	0xB99D, 0x08,
	0xB99E, 0x06,
	0xB9A0, 0x07,
	0xB9A2, 0x04,
	0xB9A3, 0x06,
	0xB9A5, 0x05,
	0xBA34, 0x14,
	0xBA35, 0x14,
	0xBA36, 0x14,
	0xBA37, 0x14,
	0xBA38, 0x14,
	0xBA39, 0x14,
	0xBA3A, 0x14,
	0xBA3B, 0x14,
	0xBA3C, 0x05,
	0xBA3D, 0x14,
	0xBA3E, 0x14,
	0xBA3F, 0x14,
	0xBA40, 0x14,
	0xBA41, 0x14,
	0xBA42, 0x14,
	0xAD6C, 0x10,
	0xAD6E, 0x10,
	0xAD70, 0x12,
	0xAD71, 0x12,
	0xB3EA, 0x02,
	0xB3EB, 0x1F,
	0xB3EE, 0x01,
	0xB3F2, 0x02,
	0xB3F3, 0x76,
	0xB3F8, 0x02,
	0xB3F9, 0x1F,
	0xB3FC, 0x01,
	0xB400, 0x02,
	0xB401, 0x76,
	0xB406, 0x02,
	0xB407, 0x1F,
	0xB40A, 0x01,
	0xB40B, 0x2A,
	0xB40E, 0x02,
	0xB40F, 0x76,
	0xB414, 0x01,
	0xB415, 0x92,
	0xB418, 0x01,
	0xB419, 0x2A,
	0xB41C, 0x02,
	0xB41D, 0x23,
	0xB422, 0x02,
	0xB423, 0x1F,
	0xB426, 0x01,
	0xB427, 0x2A,
	0xB42A, 0x02,
	0xB42B, 0x23,
	0xB42C, 0x02,
	0xB42D, 0x1F,
	0xB430, 0x01,
	0xB431, 0x2A,
	0xB434, 0x02,
	0xB435, 0x76,
	0xB436, 0x02,
	0xB437, 0x76,
	0xB43A, 0x01,
	0xB43B, 0x2A,
	0xB43E, 0x02,
	0xB43F, 0x76,
	0xA46D, 0x01,
	0xA46E, 0x01,
	0xA46F, 0x01,
	0xA470, 0x01,
	0xA471, 0x01,
	0xA472, 0x01,
	0xA473, 0x01,
	0xA474, 0x01,
	0xA475, 0x01,
	0xA476, 0x01,
	0xA477, 0x01,
	0xA478, 0x01,
	0xA479, 0x01,
	0xA47A, 0x01,
	0xA47B, 0x01,
	0xA47C, 0x01,
	0xA47D, 0x01,
	0xA47E, 0x01,
	0xA47F, 0x01,
	0xA480, 0x01,
	0xA481, 0x01,
	0xA482, 0x01,
	0xA483, 0x01,
	0xA484, 0x01,
	0xA485, 0x01,
	0xA486, 0x01,
	0xA4D5, 0x01,
	0xA4D6, 0x01,
	0xA4D7, 0x01,
	0xA4D8, 0x01,
	0xA4D9, 0x01,
	0xA4DA, 0x01,
	0xA4DB, 0x01,
	0xA4DC, 0x01,
	0xA4DD, 0x01,
	0xA4DE, 0x01,
	0xA4DF, 0x01,
	0xA4E0, 0x01,
	0xA4E1, 0x01,
	0xA4E2, 0x01,
	0xA4E3, 0x01,
	0xA4E4, 0x01,
	0xA4E5, 0x01,
	0xA4E6, 0x01,
	0xA4E7, 0x01,
	0xA4E8, 0x01,
	0xA4E9, 0x01,
	0xA4EA, 0x01,
	0xA4EB, 0x01,
	0xA4EC, 0x01,
	0xA4ED, 0x01,
	0xA4EE, 0x01,
	0xCC07, 0x00,
	0xCC0E, 0x00,
	0xA7FF, 0x07,
	0x4333, 0x00,
};

/* BIN_30FPS_ALLPD H:4096 V:3072 */
static kal_uint16 imx06c_preview_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x1D,
	0x0343, 0x60,
	0x3850, 0x00,
	0x3851, 0x78,
	/* Frame Length Lines Setting */
	0x0340, 0x1A,
	0x0341, 0x08,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x02,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xF2,
	0x38A2, 0x00,
	0x38A3, 0xF2,
	0x38A4, 0x00,
	0x38A5, 0xF2,
	0x38A6, 0x00,
	0x38A7, 0xF2,
	0x38A8, 0x00,
	0x38A9, 0x50,
	0x38AA, 0x00,
	0x38AB, 0x50,
	0x38AC, 0x00,
	0x38AD, 0x50,
	0x38AE, 0x00,
	0x38AF, 0x50,
	0x38D0, 0x02,
	0x38D1, 0x7E,
	0x38D2, 0x00,
	0x38D3, 0xAE,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x13,
	0x0205, 0x34,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* BIN_30FPS_ALLPD H:4096 V:3072 */
static kal_uint16 imx06c_capture_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x1D,
	0x0343, 0x60,
	0x3850, 0x00,
	0x3851, 0x78,
	/* Frame Length Lines Setting */
	0x0340, 0x1A,
	0x0341, 0x08,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x02,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xF2,
	0x38A2, 0x00,
	0x38A3, 0xF2,
	0x38A4, 0x00,
	0x38A5, 0xF2,
	0x38A6, 0x00,
	0x38A7, 0xF2,
	0x38A8, 0x00,
	0x38A9, 0x50,
	0x38AA, 0x00,
	0x38AB, 0x50,
	0x38AC, 0x00,
	0x38AD, 0x50,
	0x38AE, 0x00,
	0x38AF, 0x50,
	0x38D0, 0x02,
	0x38D1, 0x7E,
	0x38D2, 0x00,
	0x38D3, 0xAE,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x13,
	0x0205, 0x34,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* BIN_CROP_30FPS_ALLPD_S1 H:4096 V:2304 */
static kal_uint16 imx06c_normal_video_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x1D,
	0x0343, 0x60,
	0x3850, 0x00,
	0x3851, 0x78,
	/* Frame Length Lines Setting */
	0x0340, 0x1A,
	0x0341, 0x08,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x14,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x02,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x09,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x09,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xF2,
	0x38A2, 0x00,
	0x38A3, 0xF2,
	0x38A4, 0x00,
	0x38A5, 0xF2,
	0x38A6, 0x00,
	0x38A7, 0xF2,
	0x38A8, 0x00,
	0x38A9, 0x50,
	0x38AA, 0x00,
	0x38AB, 0x50,
	0x38AC, 0x00,
	0x38AD, 0x50,
	0x38AE, 0x00,
	0x38AF, 0x50,
	0x38D0, 0x02,
	0x38D1, 0x7E,
	0x38D2, 0x00,
	0x38D3, 0xAE,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x13,
	0x0205, 0x34,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* BIN_CROP_60FPS_ALLPD H:4096 V:2304 */
static kal_uint16 imx06c_hs_video_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x1D,
	0x0343, 0x60,
	0x3850, 0x00,
	0x3851, 0x78,
	/* Frame Length Lines Setting */
	0x0340, 0x0D,
	0x0341, 0x04,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x14,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x02,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x09,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x09,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xF2,
	0x38A2, 0x00,
	0x38A3, 0xF2,
	0x38A4, 0x00,
	0x38A5, 0xF2,
	0x38A6, 0x00,
	0x38A7, 0xF2,
	0x38A8, 0x00,
	0x38A9, 0x50,
	0x38AA, 0x00,
	0x38AB, 0x50,
	0x38AC, 0x00,
	0x38AD, 0x50,
	0x38AE, 0x00,
	0x38AF, 0x50,
	0x38D0, 0x02,
	0x38D1, 0x7E,
	0x38D2, 0x00,
	0x38D3, 0xAE,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x13,
	0x0205, 0x34,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* V2H2_CROP_120FPS_NO_PD H:1280 V:720  */
static kal_uint16 imx06c_slim_video_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x30,
	0x0343, 0x20,
	0x3850, 0x00,
	0x3851, 0x78,
	/* Frame Length Lines Setting */
	0x0340, 0x06,
	0x0341, 0x80,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x06,
	0x0347, 0x60,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x11,
	0x034B, 0x9F,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x44,
	0x0902, 0x02,
	0x3005, 0x00,
	0x3006, 0x00,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x00,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x43,
	0x31E5, 0x43,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x01,
	0x0409, 0x80,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x05,
	0x040D, 0x00,
	0x040E, 0x02,
	0x040F, 0xD0,
	/* Output Size Setting */
	0x034C, 0x05,
	0x034D, 0x00,
	0x034E, 0x02,
	0x034F, 0xD0,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x9B,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x00,
	0x3205, 0x00,
	0x3206, 0x00,
	0x3211, 0x00,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x01,
	0x38A1, 0x58,
	0x38A2, 0x01,
	0x38A3, 0x58,
	0x38A4, 0x01,
	0x38A5, 0x58,
	0x38A6, 0x01,
	0x38A7, 0x58,
	0x38A8, 0x0A,
	0x38A9, 0x22,
	0x38AA, 0x0A,
	0x38AB, 0x22,
	0x38AC, 0x0A,
	0x38AD, 0x22,
	0x38AE, 0x0A,
	0x38AF, 0x22,
	0x38D0, 0x05,
	0x38D1, 0x5C,
	0x38D2, 0x01,
	0x38D3, 0x88,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x13,
	0x0205, 0x34,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x00,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x30,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* DCG_VBIN_RATIO4_RAW12_30FPS_ALLPD_PPDOFF_S1, HSG H:4096 V:2304 */
static kal_uint16 imx06c_custom1_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0C,
	0x0113, 0x0C,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x30,
	0x0343, 0x10,
	0x3850, 0x00,
	0x3851, 0xC3,
	/* Frame Length Lines Setting */
	0x0340, 0x10,
	0x0341, 0x04,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x14,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x04,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x01,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x09,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x09,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xA8,
	0x38A2, 0x00,
	0x38A3, 0xA8,
	0x38A4, 0x00,
	0x38A5, 0xA8,
	0x38A6, 0x00,
	0x38A7, 0xA8,
	0x38A8, 0x00,
	0x38A9, 0xA8,
	0x38AA, 0x00,
	0x38AB, 0xA8,
	0x38AC, 0x00,
	0x38AD, 0xA8,
	0x38AE, 0x00,
	0x38AF, 0xA8,
	0x38D0, 0x01,
	0x38D1, 0x80,
	0x38D2, 0x01,
	0x38D3, 0x80,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x34,
	0x0205, 0xD0,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* DCG_VBIN_RATIO4_RAW12_30FPS_ALLPD_PPDOFF_S2, HSG H:4096 V:3072 */
static kal_uint16 imx06c_custom2_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0C,
	0x0113, 0x0C,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x30,
	0x0343, 0x10,
	0x3850, 0x00,
	0x3851, 0xC3,
	/* Frame Length Lines Setting */
	0x0340, 0x10,
	0x0341, 0x04,
	/* ROI Setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x3005, 0x02,
	0x3006, 0x04,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x04,
	0x3180, 0x01,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x41,
	0x31E5, 0x41,
	0x320B, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x00,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x00,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0xA8,
	0x38A2, 0x00,
	0x38A3, 0xA8,
	0x38A4, 0x00,
	0x38A5, 0xA8,
	0x38A6, 0x00,
	0x38A7, 0xA8,
	0x38A8, 0x00,
	0x38A9, 0xA8,
	0x38AA, 0x00,
	0x38AB, 0xA8,
	0x38AC, 0x00,
	0x38AD, 0xA8,
	0x38AE, 0x00,
	0x38AF, 0xA8,
	0x38D0, 0x01,
	0x38D1, 0x80,
	0x38D2, 0x01,
	0x38D3, 0x80,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x34,
	0x0205, 0xD0,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F,
};

/* FULL_CROP_RMSC_30FPS_ALLPD_S2, HSG H:4096 V:3072 */
static kal_uint16 imx06c_custom3_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x24,
	0x0343, 0xD0,
	0x3850, 0x00,
	0x3851, 0x96,
	/* Frame Length Lines Setting */
	0x0340, 0x14,
	0x0341, 0xD4,
	/* ROI Setting */
	0x0344, 0x08,
	0x0345, 0x00,
	0x0346, 0x06,
	0x0347, 0x00,
	0x0348, 0x17,
	0x0349, 0xFF,
	0x034A, 0x11,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x02,
	0x3005, 0x00,
	0x3006, 0x00,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x00,
	0x3180, 0x00,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x01,
	0x31E5, 0x01,
	0x320B, 0x00,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x01,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x01,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0x8A,
	0x38A2, 0x00,
	0x38A3, 0x8A,
	0x38A4, 0x00,
	0x38A5, 0x8A,
	0x38A6, 0x00,
	0x38A7, 0x8A,
	0x38A8, 0x02,
	0x38A9, 0xB4,
	0x38AA, 0x02,
	0x38AB, 0xB4,
	0x38AC, 0x02,
	0x38AD, 0xB4,
	0x38AE, 0x02,
	0x38AF, 0xB4,
	0x38D0, 0x01,
	0x38D1, 0xBC,
	0x38D2, 0x03,
	0x38D3, 0xC2,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F
};

/* FULL_DCG_ROI_RMSC_RATIO4_RAW12_ALLPD_HSG_S2, HSG H:4096 V:3072 */
static kal_uint16 imx06c_custom4_setting[] = {
	/* MIPI output setting */
	0x0112, 0x0C,
	0x0113, 0x0C,
	0x0114, 0x02,
	/* Line Length PCK Setting */
	0x0342, 0x48,
	0x0343, 0xF0,
	0x3850, 0x01,
	0x3851, 0x28,
	/* Frame Length Lines Setting */
	0x0340, 0x0D,
	0x0341, 0x32,
	/* ROI Setting */
	0x0344, 0x08,
	0x0345, 0x00,
	0x0346, 0x06,
	0x0347, 0x00,
	0x0348, 0x17,
	0x0349, 0xFF,
	0x034A, 0x11,
	0x034B, 0xFF,
	/* Mode Setting */
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x02,
	0x3005, 0x00,
	0x3006, 0x00,
	0x3140, 0x0A,
	0x3144, 0x00,
	0x3146, 0x00,
	0x3148, 0x00,
	0x3180, 0x01,
	0x3188, 0x00,
	0x3190, 0x00,
	0x31E4, 0x01,
	0x31E5, 0x01,
	0x320B, 0x01,
	/* Digital Crop & Scaling */
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,
	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,
	/* Clock Setting */
	0x0301, 0x06,
	0x0303, 0x02,
	0x0305, 0x02,
	0x0306, 0x00,
	0x0307, 0x5F,
	0x030B, 0x02,
	0x030D, 0x02,
	0x030E, 0x00,
	0x030F, 0xC8,
	/* Other Setting */
	0x3104, 0x01,
	0x3205, 0x01,
	0x3206, 0x01,
	0x3211, 0x01,
	0x3855, 0x01,
	0x39AC, 0x01,
	0x38A0, 0x00,
	0x38A1, 0x54,
	0x38A2, 0x00,
	0x38A3, 0x54,
	0x38A4, 0x00,
	0x38A5, 0x54,
	0x38A6, 0x00,
	0x38A7, 0x54,
	0x38A8, 0x00,
	0x38A9, 0x54,
	0x38AA, 0x00,
	0x38AB, 0x54,
	0x38AC, 0x00,
	0x38AD, 0x54,
	0x38AE, 0x00,
	0x38AF, 0x54,
	0x38D0, 0x01,
	0x38D1, 0xBC,
	0x38D2, 0x01,
	0x38D3, 0xBC,
	0x97C0, 0x02,
	0x97C1, 0x30,
	/* Integration Setting */
	0x0202, 0x03,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	/* Gain Setting */
	0x0204, 0x30,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x3174, 0x00,
	0x3175, 0x00,
	0x3176, 0x01,
	0x3177, 0x00,
	/* DCGHDR Setting */
	0x3181, 0x00,
	0x3182, 0x04,
	/* PHASE PIX Output Data Setting */
	0x3979, 0x00,
	0x397B, 0x02,
	0x397C, 0x01,
	/* PHASE PIX1 VCID Setting */
	0x30A4, 0x01,
	0x30F2, 0x01,
	/* PHASE PIX1 data type Setting */
	0x30A5, 0x2B,
	/* PHASE PIX2 VCID Setting */
	0x30A6, 0x00,
	0x30F3, 0x01,
	/* PHASE PIX2 data type Setting */
	0x30A7, 0x30,
	/* MIPI Global Timing Setting */
	0x084E, 0x00,
	0x084F, 0x17,
	0x0850, 0x00,
	0x0851, 0x13,
	0x0852, 0x00,
	0x0853, 0x27,
	0x0854, 0x00,
	0x0855, 0x2B,
	0x0858, 0x00,
	0x0859, 0x1F
};

static void sensor_init(void)
{
	pr_debug("[%s] E!\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_init_setting,
	    sizeof(imx06c_init_setting)/sizeof(kal_uint16));
	/*enable temperature sensor, TEMP_SEN_CTL:*/
	write_cmos_sensor_8(0x0138, 0x01);
	set_mirror_flip(imgsensor.mirror);

	pr_debug("[%s] X!\n", __func__);
}  /* sensor_init */

static void preview_setting(void)
{
	pr_debug("[%s] E!\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_preview_setting,
		sizeof(imx06c_preview_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}  /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	int _length = 0;

	_length = sizeof(imx06c_capture_setting)/sizeof(kal_uint16);
	pr_debug("[%s] E! 30 fps, currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/
	if (!imx06c_is_seamless)
		imx06c_table_write_cmos_sensor(imx06c_capture_setting, _length);
	else {
		pr_debug("%s imx06c_is_seamless %d, imx06c_size_to_write %d\n",
			__func__, imx06c_is_seamless, imx06c_size_to_write);
		if (imx06c_size_to_write + _length > _I2C_BUF_SIZE) {
			pr_debug("_too much i2c data for fast siwtch %d\n",
				imx06c_size_to_write + _length);
			return;
		}
		memcpy((void *) (imx06c_i2c_data + imx06c_size_to_write),
			imx06c_capture_setting,
			sizeof(imx06c_capture_setting));
		imx06c_size_to_write += _length;
	}

	pr_debug("[%s] X! 30 fps\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("[%s] E! currefps: %d\n", __func__, currefps);

	imx06c_table_write_cmos_sensor(imx06c_normal_video_setting,
		sizeof(imx06c_normal_video_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void hs_video_setting(void)
{
	pr_debug("[%s] E! currefps 120\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_hs_video_setting,
		sizeof(imx06c_hs_video_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void slim_video_setting(void)
{
	pr_debug("[%s] E! 4096*23046@30fps\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_slim_video_setting,
		sizeof(imx06c_slim_video_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void custom1_setting(void)
{
	pr_debug("[%s] E! DCG-4096*2304@30fps\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_custom1_setting,
		sizeof(imx06c_custom1_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void custom2_setting(void)
{
	pr_debug("[%s] E! DCG-4096*3072@30fps\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_custom2_setting,
		sizeof(imx06c_custom2_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void custom3_setting(void)
{
	pr_debug("[%s] E! ISZ-4096*3072@30fps\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_custom3_setting,
		sizeof(imx06c_custom3_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

static void custom4_setting(void)
{
	pr_debug("[%s] E! ISZ+DCG-4096*3072@24fps\n", __func__);

	imx06c_table_write_cmos_sensor(imx06c_custom4_setting,
		sizeof(imx06c_custom4_setting)/sizeof(kal_uint16));

	pr_debug("[%s] X!\n", __func__);
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	pr_debug("[%s] E!\n", __func__);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			pr_debug(
				"read_0x0016=0x%X, 0x0017=0x%X,sensor id(Rev/Exp):0x%X / 0x%X\n",
				read_cmos_sensor_8(0x0016),
				read_cmos_sensor_8(0x0017),
				*sensor_id,
				imgsensor_info.sensor_id);
			// *sensor_id = 0x06C;
			// *sensor_id = imgsensor_info.sensor_id;
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("[%s] i2c write id: 0x%X, sensor id: 0x%X\n",
					__func__, imgsensor.i2c_write_id, *sensor_id);
				read_sensor_Cali();
				return ERROR_NONE;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	pr_debug("[%s] E!\n", __func__);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("[%s] X!\n", __func__);

	return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	pr_debug("[%s] E\n", __func__);
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	qsc_flag = 0;
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
} /* close */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
} /* preview */


/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E!\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E!\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E!\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E! 720P@240FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* slim_video */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E!DCG sensor merge@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E!DCG sensor merge 4096*3072@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E! ISZ 4096*3072@30FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E! ISZ + DCG sensor merge 4096*3072@24FPS\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	set_mirror_flip(imgsensor.mirror);
	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* custom2 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	pr_debug("[%s] E!\n", __func__);
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom6Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom6Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom7Width =
		imgsensor_info.pre.grabwindow_width;

	sensor_resolution->SensorCustom7Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom8Width =
		imgsensor_info.pre.grabwindow_width;

	sensor_resolution->SensorCustom8Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom9Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom9Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom10Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom10Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom11Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom11Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom12Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom12Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom13Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom13Height =
		imgsensor_info.pre.grabwindow_height;


	sensor_resolution->SensorCustom14Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom14Height =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorCustom15Width =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorCustom15Height =
		imgsensor_info.pre.grabwindow_height;

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E! scenario_id = %d\n", __func__, scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom4.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/*	get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("[%s] E! scenario_id = %d\n", __func__, scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {

	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	pr_debug("[%s] X!\n", __func__);
	return ERROR_NONE;
}	/* control() */


static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	kal_uint32 shutter, kal_uint32 gain,
	kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	//int k = 0;
	imx06c_is_seamless = true;
	memset(imx06c_i2c_data, 0x0, sizeof(imx06c_i2c_data));
	imx06c_size_to_write = 0;

	pr_debug("[%s] E! %d, %d, %d, %d, %d sizeof(imx06c_i2c_data) %lu\n",
		__func__, scenario_id, shutter, gain,
		shutter_2ndframe, gain_2ndframe,
		sizeof(imx06c_i2c_data));

	if (scenario_id != MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG &&
		scenario_id != MSDK_SCENARIO_ID_CUSTOM4 &&
		scenario_id != MSDK_SCENARIO_ID_CUSTOM3)
		return ERROR_INVALID_SCENARIO_ID;


	imx06c_i2c_data[imx06c_size_to_write++] = 0x0104;
	imx06c_i2c_data[imx06c_size_to_write++] = 0x01;

	control(scenario_id, NULL, NULL);
	if (shutter != 0)
		set_shutter(shutter);
	if (gain != 0)
		set_gain(gain);

	imx06c_i2c_data[imx06c_size_to_write++] = 0x0104;
	imx06c_i2c_data[imx06c_size_to_write++] = 0;

	pr_debug("%s imx06c_is_seamless %d, imx06c_size_to_write %d\n",
				__func__, imx06c_is_seamless, imx06c_size_to_write);

	imx06c_table_write_cmos_sensor(
		imx06c_i2c_data,
		imx06c_size_to_write);

	imx06c_is_seamless = false;
	pr_debug("exit\n");
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/ {
		imgsensor.autoflicker_en = KAL_TRUE;
		pr_debug("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		pr_debug(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
				+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
			/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
			/ imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom2.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
			/ imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
			? (frame_length - imgsensor_info.custom3.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom3.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
			/ imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
			? (frame_length - imgsensor_info.custom4.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom4.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	pr_debug("set_test_pattern enum: %d\n", modes);

	if (modes) {
		write_cmos_sensor_8(0x0601, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			pr_debug("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
				pdata->COLOR_R, pdata->COLOR_Gr, pdata->COLOR_B, pdata->COLOR_Gb);
			Color_R = (pdata->COLOR_R >> 22) & 0x3FF; //10bits depth color
			Color_Gr = (pdata->COLOR_Gr >> 22) & 0x3FF;
			Color_B = (pdata->COLOR_B >> 22) & 0x3FF;
			Color_Gb = (pdata->COLOR_Gb >> 22) & 0x3FF;
			write_cmos_sensor_8(0x0602, (Color_R >> 8) & 0x3);
			write_cmos_sensor_8(0x0603, Color_R & 0xFF);
			write_cmos_sensor_8(0x0604, (Color_Gr >> 8) & 0x3);
			write_cmos_sensor_8(0x0605, Color_Gr & 0xFF);
			write_cmos_sensor_8(0x0606, (Color_B >> 8) & 0x3);
			write_cmos_sensor_8(0x0607, Color_B & 0xFF);
			write_cmos_sensor_8(0x0608, (Color_Gb >> 8) & 0x3);
			write_cmos_sensor_8(0x0609, Color_Gb & 0xFF);
		}
	} else {
		write_cmos_sensor_8(0x0601, 0x00); /*No pattern*/
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	/* LOG_INF("temp_c(%d), read_reg(%d)\n", */
	/* temperature_convert, temperature); */

	return temperature_convert;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	kal_uint8 index = 0;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	pr_debug("[%s]E, feature_id = %d\n", __func__, feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		if (IS_MT6893(g_platform_id) || IS_MT6885(g_platform_id))
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 5693360;
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		if (sensor_reg_data->RegAddr == 0xff) {
			seamless_switch(sensor_reg_data->RegData, 0, 0, 0, 0);
		} else {
			write_cmos_sensor_8(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		}
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo =
		(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[7],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[8],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 186;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:  //4096*2304
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 162;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO: // 1280*720
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 140;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1: // 4096*2304
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 162;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2: // 4096*3072
			imgsensor_pd_info_binning.i4BlockNumX = 496;
			imgsensor_pd_info_binning.i4BlockNumY = 162;
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info_binning,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d\n",
			(*feature_para_len));
		imx06c_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		imx06c_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
					   , feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1; /*BINNING_NONE*/
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	}
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);

		pvcinfo =
			(struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[7],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[8],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			pr_info("error: get wrong vc_INFO id = %d",
			*feature_data_32);
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
	{
		/* modify to separate 3hdr and remosaic */
		spin_lock(&imgsensor_drv_lock);
		if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM3) {
			/*write AWB gain to sensor*/
			feedback_awbgain((UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
		} else {
			imx06c_awb_gain(
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		spin_unlock(&imgsensor_drv_lock);
		break;
	}
	case SENSOR_FEATURE_SET_LSC_TBL:
	{
		index = *(((kal_uint8 *)feature_para) + (*feature_para_len));
		imx06c_set_lsc_reg_setting(index, feature_data_16,
			(*feature_para_len)/sizeof(UINT16));
		break;
	}
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
	{
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			pr_debug(
			"warning! no ae_ctrl input");
		}
		if (feature_data == NULL) {
			pr_info("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}
		if (pAeCtrls != NULL) {
			seamless_switch((*feature_data),
					*pAeCtrls, *(pAeCtrls + 1),
					*(pAeCtrls + 4), *(pAeCtrls + 5));
		} else {
			seamless_switch((*feature_data),
					0, 0, 0, 0);
		}
		break;
	}
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
	{
		if ((feature_data + 1) != NULL) {
			pScenarios =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			pr_info("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*pScenarios = MSDK_SCENARIO_ID_CUSTOM3;
			*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM4;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			*pScenarios = 0xff;
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %d\n",
		*feature_data, *pScenarios);
		break;
	}
	case SENSOR_FEATURE_GET_SEAMLESS_SYSTEM_DELAY:
	{
		*(feature_data + 2) = 0;
		for (int i = 0;
			i < sizeof(seamless_sys_delays) / sizeof(struct SEAMLESS_SYS_DELAY);
			i++) {
			if (*feature_data == seamless_sys_delays[i].source_scenario &&
		    *(feature_data + 1) == seamless_sys_delays[i].target_scenario) {
				*(feature_data + 2) = seamless_sys_delays[i].sys_delay;
				break;
			}
		}
		break;
	}
	case SENSOR_FEATURE_SET_HDR_SHUTTER://for 2EXP
	{
		pr_info("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write shutter for NE/SE
		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM1 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM4)
			set_shutter((UINT16) *feature_data);
		break;
	}
	case SENSOR_FEATURE_SET_DUAL_GAIN://for 2EXP
	{
		pr_info("SENSOR_FEATURE_SET_DUAL_GAIN LE=%d, SE=%d\n",
				(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		// implement write gain for NE/SE
		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM1 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM4)
			set_gain((UINT16) *feature_data);
		break;
	}
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
	{
		if (*feature_data == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_DCG_COMPOSE_RAW12:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM1;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM1) {
			switch (*(feature_data + 1)) {
			case HDR_NONE :
				*(feature_data + 2) = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
				break;
			default:
				break;
			}
		}
		if (*feature_data == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_DCG_COMPOSE_RAW12:
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;
				break;
			default:
				break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case HDR_NONE :
				*(feature_data + 2) = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
				break;
			default:
				break;
			}
		}
		pr_info("[%s][SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO] %d %d<->%d\n",
				__func__,
				(UINT16) (*feature_data),
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	}
	case SENSOR_FEATURE_SET_HDR_SHUTTER_FRAME_TIME:
	{
		pr_info("SENSOR_FEATURE_SET_HDR_SHUTTER_FRAME_TIME\n");
		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM1 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2 ||
			imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM4) {
			set_shutter_frame_length((UINT16) (*feature_data),
						(UINT16) (*(feature_data + 3)),
						1);
		}
		break;
	}
	case SENSOR_FEATURE_GET_RAW_BIT_BY_SCENARIO:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 12;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 10;
			break;
		}
		pr_info("[%s][SENSOR_FEATURE_GET_RAW_BIT_BY_SCENARIO] get mode(%llu) raw bit(%d)\n",
			__func__, *feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	case SENSOR_FEATURE_GET_SATURATION_LEVEL_BY_SCENARIO:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 3900;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 1023;
			break;
		}
		pr_info("[%s][SENSOR_FEATURE_GET_SATURATION_LEVEL_BY_SCENARIO] mode(%llu) s_max(%d)\n",
			__func__, *feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	case SENSOR_FEATURE_GET_GAIN_RATIO_BY_SCENARIO:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 4000;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 1000;
			break;
		}
		pr_info("[%s][SENSOR_FEATURE_GET_GAIN_RATIO_BY_SCENARIO] mode(%llu) ratio(%d)\n",
			__func__, *feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	case SENSOR_FEATURE_GET_DCG_GAIN_MODE_BY_SCENARIO:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= IMGSENSOR_DCG_RATIO_MODE;
			break;
		}
		pr_info("[%s][SENSOR_FEATURE_GET_DCG_GAIN_MODE_BY_SCENARIO] mode(%llu) gain_mode(%d)\n",
			__func__, *feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control() */

static void set_platform_info(unsigned int platform_id)
{
	g_platform_id = platform_id;
	pr_info("%s id:%x\n", __func__, g_platform_id);
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close,
	set_platform_info
};

UINT32 IMX06C_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	sensor_func.arch = IMGSENSOR_ARCH_V2;
	pr_info("IMX06C Load Function\n");
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	if (imgsensor.psensor_func == NULL)
		imgsensor.psensor_func = &sensor_func;
	return ERROR_NONE;
} /* IMX06C_MIPI_RAW_SensorInit */
