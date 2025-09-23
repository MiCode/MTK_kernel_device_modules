// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5kgd1mipiraw_Sensor.c
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


#define PFX "s5kgd1_camera_sensor_1817"
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

#include "s5kgd1mipiraw_Sensor.h"
#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020	/*trans# max is 255, each 3 bytes*/
#else
#define I2C_BUFFER_LEN 4
#endif

#define STEREO_CUSTOM1_30FPS 0

#define LOG_INF(format, args...) printk(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIN_VOLTAGE (0)
#define MAX_VOLTAGE (600)
//extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
#define AUXADC_CAMERA_VOLTAGE_CHANNEL   4
int CheckGmsFlag =0;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGD1_SENSOR_ID,
    .checksum_value = 0xc98e6b72,
	.pre = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.cap = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.cap1 = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.cap2 = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.normal_video = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.hs_video = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
	.slim_video = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},

#if STEREO_CUSTOM1_30FPS
	.custom1 = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
#else
	.custom1 = {
		.pclk = 1144000000,
		.linelength  = 14528,
		.framelength = 2624,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 345600000,
	},
#endif

	.margin = 10,
	.min_shutter = 8,

	.min_gain = 64,	  //1x gain
	.max_gain = 1024, //16x gain
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 2,
	.gain_type = 2,
	.max_frame_length = 0xFFFF,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,
	/*[name:imgsensor_isp6s&]FrameSyncAlgo[calc_predicted_frame_length] ERROR: [1] ID:0x1(sidx:1), frame_time_delay_frame:1 is not valid (must be 2 or 3)*/
	.frame_time_delay_frame = 2,     //1 > 2
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 6,

	.cap_delay_frame = 600,
	.pre_delay_frame = 600,
	.video_delay_frame = 600,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr, //SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 1000,
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x20,0x5a, 0xff},//0x6c 0x6d & 0x21 0x20
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

		/* test pattern mode or not.
		 * KAL_FALSE for in test pattern mode,
		 * KAL_TRUE for normal output
		 */
	.test_pattern = KAL_FALSE,


	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,


	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0x20,

};



static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
#if STEREO_CUSTOM1_30FPS
 { 6560, 4928,    0,    0, 6560, 4928, 3280, 2464,   0, 0, 3280, 2464,   0, 0, 3280, 2464},
#else
 { 6560, 4928,    0,    0, 6560, 4928, 6560, 4928,   0, 0, 6560, 4928,   0, 0, 6560, 4928},
#endif
};


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);


	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(
		puSendCmd, tosend, imgsensor.i2c_write_id, 4,
				     imgsensor_info.i2c_speed);
		tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;

#endif
	}
	return 0;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

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
}

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	LOG_INF("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor_8(0x0100, 0x01);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint16 shutter)
{

	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(0x0202, shutter & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}


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
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
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
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

    reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("set_gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));
	return gain;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
	spin_lock(&imgsensor_drv_lock);
	//image_mirror &= 3;
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, 0x00);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, 0x03);
		break;
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

static kal_uint16 addr_data_pair_init_gd1[] = {
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2001,
	0x602A,0x518C,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0549,
	0x6F12,0x0448,
	0x6F12,0x054A,
	0x6F12,0xC1F8,
	0x6F12,0xD006,
	0x6F12,0x101A,
	0x6F12,0xA1F8,
	0x6F12,0xD406,
	0x6F12,0x00F0,
	0x6F12,0x59BA,
	0x6F12,0x2001,
	0x6F12,0x5880,
	0x6F12,0x2000,
	0x6F12,0x68C0,
	0x6F12,0x2001,
	0x6F12,0xF800,
	0x6F12,0x2DE9,
	0x6F12,0xFF4F,
	0x6F12,0x0446,
	0x6F12,0x0878,
	0x6F12,0x85B0,
	0x6F12,0x9246,
	0x6F12,0x0F46,
	0x6F12,0x0028,
	0x6F12,0x6FD0,
	0x6F12,0x00F0,
	0x6F12,0xABFA,
	0x6F12,0x0028,
	0x6F12,0x6BD0,
	0x6F12,0x0026,
	0x6F12,0x00F0,
	0x6F12,0xA6FA,
	0x6F12,0x0328,
	0x6F12,0x69D0,
	0x6F12,0x00F0,
	0x6F12,0xA7FA,
	0x6F12,0x050A,
	0x6F12,0xB6FA,
	0x6F12,0x86F0,
	0x6F12,0xC0F1,
	0x6F12,0x2002,
	0x6F12,0x092A,
	0x6F12,0x00D2,
	0x6F12,0x0922,
	0x6F12,0x093A,
	0x6F12,0xD640,
	0x6F12,0xB5FA,
	0x6F12,0x85F0,
	0x6F12,0xC0F1,
	0x6F12,0x2000,
	0x6F12,0xF6B2,
	0x6F12,0x0928,
	0x6F12,0x00D2,
	0x6F12,0x0920,
	0x6F12,0x0938,
	0x6F12,0xC540,
	0x6F12,0x00EB,
	0x6F12,0x8008,
	0x6F12,0x0898,
	0x6F12,0x05F0,
	0x6F12,0xFF09,
	0x6F12,0x00EB,
	0x6F12,0xC003,
	0x6F12,0x03EB,
	0x6F12,0x0013,
	0x6F12,0x0021,
	0x6F12,0xEB46,
	0x6F12,0x07EB,
	0x6F12,0xC30C,
	0x6F12,0x01EB,
	0x6F12,0xC100,
	0x6F12,0x00EB,
	0x6F12,0x0110,
	0x6F12,0x0CEB,
	0x6F12,0x4000,
	0x6F12,0x00EB,
	0x6F12,0x4800,
	0x6F12,0x851C,
	0x6F12,0x531C,
	0x6F12,0x0C30,
	0x6F12,0x35F9,
	0x6F12,0x1370,
	0x6F12,0x30F9,
	0x6F12,0x1330,
	0x6F12,0x35F9,
	0x6F12,0x1250,
	0x6F12,0x30F9,
	0x6F12,0x1200,
	0x6F12,0x7F1B,
	0x6F12,0x1B1A,
	0x6F12,0x7743,
	0x6F12,0x7343,
	0x6F12,0x8037,
	0x6F12,0x8033,
	0x6F12,0x05EB,
	0x6F12,0x2725,
	0x6F12,0x00EB,
	0x6F12,0x2320,
	0x6F12,0x401B,
	0x6F12,0x00FB,
	0x6F12,0x09F0,
	0x6F12,0x8030,
	0x6F12,0x05EB,
	0x6F12,0x2020,
	0x6F12,0x4BF8,
	0x6F12,0x2100,
	0x6F12,0x00FA,
	0x6F12,0x0AF0,
	0x6F12,0x4BF8,
	0x6F12,0x2100,
	0x6F12,0x491C,
	0x6F12,0x0429,
	0x6F12,0xD3D3,
	0x6F12,0xD4F8,
	0x6F12,0x0001,
	0x6F12,0x0099,
	0x6F12,0x0844,
	0x6F12,0xC4F8,
	0x6F12,0x0001,
	0x6F12,0xD4F8,
	0x6F12,0x0401,
	0x6F12,0x0199,
	0x6F12,0x0844,
	0x6F12,0xC4F8,
	0x6F12,0x0401,
	0x6F12,0xD4F8,
	0x6F12,0x0801,
	0x6F12,0x0299,
	0x6F12,0x0844,
	0x6F12,0xC4F8,
	0x6F12,0x0801,
	0x6F12,0xD4F8,
	0x6F12,0x0C01,
	0x6F12,0x0399,
	0x6F12,0x0844,
	0x6F12,0xC4F8,
	0x6F12,0x0C01,
	0x6F12,0x09B0,
	0x6F12,0xBDE8,
	0x6F12,0xF08F,
	0x6F12,0x00F0,
	0x6F12,0x42FA,
	0x6F12,0x050A,
	0x6F12,0x00F0,
	0x6F12,0x44FA,
	0x6F12,0x060A,
	0x6F12,0x91E7,
	0x6F12,0x2DE9,
	0x6F12,0xF843,
	0x6F12,0x1C46,
	0x6F12,0x0093,
	0x6F12,0x0E46,
	0x6F12,0x1546,
	0x6F12,0x1346,
	0x6F12,0x0746,
	0x6F12,0x0022,
	0x6F12,0xFD49,
	0x6F12,0x00F0,
	0x6F12,0x3BFA,
	0x6F12,0xFC49,
	0x6F12,0x2B46,
	0x6F12,0x0422,
	0x6F12,0x1831,
	0x6F12,0x3846,
	0x6F12,0x0094,
	0x6F12,0x00F0,
	0x6F12,0x33FA,
	0x6F12,0xF849,
	0x6F12,0x2B46,
	0x6F12,0x0822,
	0x6F12,0x3031,
	0x6F12,0x3846,
	0x6F12,0x0094,
	0x6F12,0x00F0,
	0x6F12,0x2BFA,
	0x6F12,0x96F8,
	0x6F12,0x5A02,
	0x6F12,0x0028,
	0x6F12,0x69D0,
	0x6F12,0x00F0,
	0x6F12,0x11FA,
	0x6F12,0x0028,
	0x6F12,0x65D0,
	0x6F12,0xF14D,
	0x6F12,0x4FF0,
	0x6F12,0x0008,
	0x6F12,0x00F0,
	0x6F12,0x0AFA,
	0x6F12,0x0328,
	0x6F12,0x60D0,
	0x6F12,0x0923,
	0x6F12,0x1022,
	0x6F12,0x2968,
	0x6F12,0x686A,
	0x6F12,0x00F0,
	0x6F12,0x1BFA,
	0x6F12,0x070A,
	0x6F12,0xB7FA,
	0x6F12,0x87F0,
	0x6F12,0xC0F1,
	0x6F12,0x2001,
	0x6F12,0x0929,
	0x6F12,0x00D2,
	0x6F12,0x0921,
	0x6F12,0xB8FA,
	0x6F12,0x88F0,
	0x6F12,0xC0F1,
	0x6F12,0x2005,
	0x6F12,0x0939,
	0x6F12,0x092D,
	0x6F12,0x00D2,
	0x6F12,0x0925,
	0x6F12,0x0020,
	0x6F12,0x01EB,
	0x6F12,0x8107,
	0x6F12,0x093D,
	0x6F12,0x00EB,
	0x6F12,0xC001,
	0x6F12,0x01EB,
	0x6F12,0x0011,
	0x6F12,0x06EB,
	0x6F12,0x4101,
	0x6F12,0x01EB,
	0x6F12,0x4701,
	0x6F12,0x01EB,
	0x6F12,0x4501,
	0x6F12,0x54F8,
	0x6F12,0x2020,
	0x6F12,0xB1F8,
	0x6F12,0x5C12,
	0x6F12,0x4A43,
	0x6F12,0x110B,
	0x6F12,0x44F8,
	0x6F12,0x2010,
	0x6F12,0x401C,
	0x6F12,0x0428,
	0x6F12,0xEAD3,
	0x6F12,0x0020,
	0x6F12,0x00EB,
	0x6F12,0xC001,
	0x6F12,0x01EB,
	0x6F12,0x0011,
	0x6F12,0x06EB,
	0x6F12,0x4101,
	0x6F12,0x01EB,
	0x6F12,0x4701,
	0x6F12,0x01EB,
	0x6F12,0x4501,
	0x6F12,0xB1F8,
	0x6F12,0xEC23,
	0x6F12,0x04EB,
	0x6F12,0x8001,
	0x6F12,0x401C,
	0x6F12,0x0B69,
	0x6F12,0x5343,
	0x6F12,0x1A0B,
	0x6F12,0x0A61,
	0x6F12,0x0428,
	0x6F12,0xEAD3,
	0x6F12,0x00F0,
	0x6F12,0xC1F9,
	0x6F12,0x0328,
	0x6F12,0x15D1,
	0x6F12,0x0020,
	0x6F12,0x00EB,
	0x6F12,0xC001,
	0x6F12,0x01EB,
	0x6F12,0x0011,
	0x6F12,0x06EB,
	0x6F12,0x4101,
	0x6F12,0x01EB,
	0x6F12,0x4701,
	0x6F12,0x01EB,
	0x6F12,0x4501,
	0x6F12,0xB1F8,
	0x6F12,0x2423,
	0x6F12,0x04EB,
	0x6F12,0x8001,
	0x6F12,0x401C,
	0x6F12,0x0B6A,
	0x6F12,0x5343,
	0x6F12,0x1A0B,
	0x6F12,0x0A62,
	0x6F12,0x0428,
	0x6F12,0xEAD3,
	0x6F12,0xBDE8,
	0x6F12,0xF883,
	0x6F12,0x0923,
	0x6F12,0x1022,
	0x6F12,0xA96C,
	0x6F12,0x686A,
	0x6F12,0x00F0,
	0x6F12,0xBAF9,
	0x6F12,0x070A,
	0x6F12,0x0923,
	0x6F12,0x1022,
	0x6F12,0x2968,
	0x6F12,0xA86C,
	0x6F12,0x00F0,
	0x6F12,0xB3F9,
	0x6F12,0x4FEA,
	0x6F12,0x1028,
	0x6F12,0x95E7,
	0x6F12,0x70B5,
	0x6F12,0x0646,
	0x6F12,0xB548,
	0x6F12,0x0022,
	0x6F12,0xC168,
	0x6F12,0x0C0C,
	0x6F12,0x8DB2,
	0x6F12,0x2946,
	0x6F12,0x2046,
	0x6F12,0x00F0,
	0x6F12,0xAAF9,
	0x6F12,0x3046,
	0x6F12,0x00F0,
	0x6F12,0xACF9,
	0x6F12,0x0122,
	0x6F12,0x2946,
	0x6F12,0x2046,
	0x6F12,0x00F0,
	0x6F12,0xA2F9,
	0x6F12,0xAD49,
	0x6F12,0x4FF4,
	0x6F12,0x8060,
	0x6F12,0x0880,
	0x6F12,0xAB4A,
	0x6F12,0x4FF4,
	0x6F12,0xD661,
	0x6F12,0x921C,
	0x6F12,0x1180,
	0x6F12,0x911C,
	0x6F12,0x0880,
	0x6F12,0x121D,
	0x6F12,0x40F2,
	0x6F12,0x9C51,
	0x6F12,0x1180,
	0x6F12,0x921C,
	0x6F12,0x1080,
	0x6F12,0x921C,
	0x6F12,0x1180,
	0x6F12,0x921C,
	0x6F12,0x1080,
	0x6F12,0x921C,
	0x6F12,0x1180,
	0x6F12,0x921C,
	0x6F12,0x1080,
	0x6F12,0x901C,
	0x6F12,0x0180,
	0x6F12,0x70BD,
	0x6F12,0xA04B,
	0x6F12,0x10B5,
	0x6F12,0xD3F8,
	0x6F12,0x7424,
	0x6F12,0x002A,
	0x6F12,0x0ED0,
	0x6F12,0xD3F8,
	0x6F12,0x0843,
	0x6F12,0x00FB,
	0x6F12,0x0410,
	0x6F12,0x9C49,
	0x6F12,0x8C88,
	0x6F12,0x4443,
	0x6F12,0xC888,
	0x6F12,0xD3F8,
	0x6F12,0x1413,
	0x6F12,0xB4FB,
	0x6F12,0xF0F0,
	0x6F12,0x401A,
	0x6F12,0xB0FB,
	0x6F12,0xF2F0,
	0x6F12,0x10BD,
	0x6F12,0x2DE9,
	0x6F12,0xFF4F,
	0x6F12,0x89B0,
	0x6F12,0x9146,
	0x6F12,0x16AA,
	0x6F12,0x9149,
	0x6F12,0x92E8,
	0x6F12,0x0111,
	0x6F12,0xDDE9,
	0x6F12,0x1A47,
	0x6F12,0xDDE9,
	0x6F12,0x1D6A,
	0x6F12,0x0978,
	0x6F12,0xDDF8,
	0x6F12,0x64B0,
	0x6F12,0x1D46,
	0x6F12,0x11B1,
	0x6F12,0x2146,
	0x6F12,0xFFF7,
	0x6F12,0xD6FF,
	0x6F12,0x8D49,
	0x6F12,0x0AF0,
	0x6F12,0xFF03,
	0x6F12,0x1FFA,
	0x6F12,0x89F2,
	0x6F12,0xA1F8,
	0x6F12,0x9E51,
	0x6F12,0xC1F8,
	0x6F12,0x8C01,
	0x6F12,0xA1F8,
	0x6F12,0x9081,
	0x6F12,0xA1F8,
	0x6F12,0xC2C1,
	0x6F12,0xC1F8,
	0x6F12,0xB0B1,
	0x6F12,0xA1F8,
	0x6F12,0xB441,
	0x6F12,0xA1F8,
	0x6F12,0xE671,
	0x6F12,0x1C98,
	0x6F12,0xC1F8,
	0x6F12,0xD401,
	0x6F12,0xA1F8,
	0x6F12,0xD861,
	0x6F12,0x01F5,
	0x6F12,0xC271,
	0x6F12,0x0C46,
	0x6F12,0x6846,
	0x6F12,0x00F0,
	0x6F12,0x48F9,
	0x6F12,0x2146,
	0x6F12,0x6846,
	0x6F12,0x1F9A,
	0x6F12,0x00F0,
	0x6F12,0x48F9,
	0x6F12,0x7D48,
	0x6F12,0x0078,
	0x6F12,0x38B1,
	0x6F12,0xDDE9,
	0x6F12,0x0912,
	0x6F12,0x6846,
	0x6F12,0x00F0,
	0x6F12,0x45F9,
	0x6F12,0x08B1,
	0x6F12,0x00F0,
	0x6F12,0x47F9,
	0x6F12,0x0021,
	0x6F12,0x7148,
	0x6F12,0x00F0,
	0x6F12,0x48F9,
	0x6F12,0x6F48,
	0x6F12,0x0121,
	0x6F12,0x2430,
	0x6F12,0x00F0,
	0x6F12,0x43F9,
	0x6F12,0x6D48,
	0x6F12,0x0221,
	0x6F12,0x4830,
	0x6F12,0x00F0,
	0x6F12,0x3EF9,
	0x6F12,0x0DB0,
	0x6F12,0xC3E6,
	0x6F12,0x6E48,
	0x6F12,0x90F8,
	0x6F12,0xC110,
	0x6F12,0x19B1,
	0x6F12,0x90F8,
	0x6F12,0x5700,
	0x6F12,0x1128,
	0x6F12,0x01D0,
	0x6F12,0x0020,
	0x6F12,0x7047,
	0x6F12,0x0120,
	0x6F12,0x7047,
	0x6F12,0x10B5,
	0x6F12,0xE8B1,
	0x6F12,0x6949,
	0x6F12,0x2822,
	0x6F12,0xA1F5,
	0x6F12,0x5670,
	0x6F12,0x0446,
	0x6F12,0x00F0,
	0x6F12,0x2CF9,
	0x6F12,0x604A,
	0x6F12,0x6249,
	0x6F12,0x0020,
	0x6F12,0x1070,
	0x6F12,0x0888,
	0x6F12,0x50B1,
	0x6F12,0x4888,
	0x6F12,0x30B9,
	0x6F12,0xFFF7,
	0x6F12,0xE1FF,
	0x6F12,0x28B1,
	0x6F12,0x5C48,
	0x6F12,0x90F8,
	0x6F12,0x7E04,
	0x6F12,0x08B9,
	0x6F12,0x0120,
	0x6F12,0x1070,
	0x6F12,0x00F0,
	0x6F12,0x1EF9,
	0x6F12,0x2046,
	0x6F12,0x00F0,
	0x6F12,0x20F9,
	0x6F12,0x06E0,
	0x6F12,0x5A49,
	0x6F12,0x0E22,
	0x6F12,0x1831,
	0x6F12,0xA1F5,
	0x6F12,0x5670,
	0x6F12,0x00F0,
	0x6F12,0x0EF9,
	0x6F12,0x00F0,
	0x6F12,0x1BF9,
	0x6F12,0x00F0,
	0x6F12,0x1EF9,
	0x6F12,0x00F0,
	0x6F12,0x21F9,
	0x6F12,0xBDE8,
	0x6F12,0x1040,
	0x6F12,0x00F0,
	0x6F12,0x22B9,
	0x6F12,0x70B5,
	0x6F12,0x524D,
	0x6F12,0x534C,
	0x6F12,0x2888,
	0x6F12,0x2081,
	0x6F12,0xA081,
	0x6F12,0x00F0,
	0x6F12,0x1FF9,
	0x6F12,0x2080,
	0x6F12,0x2888,
	0x6F12,0xE080,
	0x6F12,0x0320,
	0x6F12,0x00F0,
	0x6F12,0x1EF9,
	0x6F12,0x0820,
	0x6F12,0x00F0,
	0x6F12,0x20F9,
	0x6F12,0x4C4D,
	0x6F12,0xA080,
	0x6F12,0x287D,
	0x6F12,0x0328,
	0x6F12,0x06D1,
	0x6F12,0x0020,
	0x6F12,0x2875,
	0x6F12,0x4A48,
	0x6F12,0x00F0,
	0x6F12,0x1BF9,
	0x6F12,0x00F0,
	0x6F12,0x1EF9,
	0x6F12,0x287D,
	0x6F12,0x58BB,
	0x6F12,0x474C,
	0x6F12,0x94F8,
	0x6F12,0x6709,
	0x6F12,0x68B9,
	0x6F12,0x4648,
	0x6F12,0x0078,
	0x6F12,0x50B9,
	0x6F12,0x94F8,
	0x6F12,0x6C09,
	0x6F12,0x38B9,
	0x6F12,0x3A48,
	0x6F12,0x007A,
	0x6F12,0x10B1,
	0x6F12,0x94F8,
	0x6F12,0x5C09,
	0x6F12,0x08B9,
	0x6F12,0x687D,
	0x6F12,0x18B1,
	0x6F12,0x1021,
	0x6F12,0x3D48,
	0x6F12,0x00F0,
	0x6F12,0x0BF9,
	0x6F12,0x94F8,
	0x6F12,0x6909,
	0x6F12,0x08B9,
	0x6F12,0x687D,
	0x6F12,0x18B1,
	0x6F12,0x4021,
	0x6F12,0x3848,
	0x6F12,0x00F0,
	0x6F12,0x02F9,
	0x6F12,0x00F0,
	0x6F12,0x05F9,
	0x6F12,0x08B9,
	0x6F12,0x687D,
	0x6F12,0x38B1,
	0x6F12,0x0C21,
	0x6F12,0x3448,
	0x6F12,0x00F0,
	0x6F12,0xF9F8,
	0x6F12,0x0121,
	0x6F12,0x3248,
	0x6F12,0x00F0,
	0x6F12,0xF5F8,
	0x6F12,0x00F0,
	0x6F12,0xFDF8,
	0x6F12,0x687D,
	0x6F12,0x0028,
	0x6F12,0x04D0,
	0x6F12,0xBDE8,
	0x6F12,0x7040,
	0x6F12,0x3048,
	0x6F12,0x00F0,
	0x6F12,0xFAB8,
	0x6F12,0x70BD,
	0x6F12,0x8908,
	0x6F12,0x8900,
	0x6F12,0x41EA,
	0x6F12,0x4000,
	0x6F12,0x82B2,
	0x6F12,0x3E21,
	0x6F12,0x46F2,
	0x6F12,0x4420,
	0x6F12,0x00F0,
	0x6F12,0xF4B8,
	0x6F12,0x10B5,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0xAB41,
	0x6F12,0x2848,
	0x6F12,0x00F0,
	0x6F12,0xF2F8,
	0x6F12,0x184C,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0xB531,
	0x6F12,0x6060,
	0x6F12,0x2548,
	0x6F12,0x00F0,
	0x6F12,0xEAF8,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0x8921,
	0x6F12,0xA060,
	0x6F12,0x2248,
	0x6F12,0x00F0,
	0x6F12,0xE3F8,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0x0F21,
	0x6F12,0xE060,
	0x6F12,0x2048,
	0x6F12,0x00F0,
	0x6F12,0xDCF8,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0x5F11,
	0x6F12,0x1E48,
	0x6F12,0x00F0,
	0x6F12,0xD6F8,
	0x6F12,0x0020,
	0x6F12,0x2146,
	0x6F12,0x0122,
	0x6F12,0x0870,
	0x6F12,0xAFF2,
	0x6F12,0x1111,
	0x6F12,0x1A48,
	0x6F12,0x00F0,
	0x6F12,0xCDF8,
	0x6F12,0x0022,
	0x6F12,0xAFF2,
	0x6F12,0x7101,
	0x6F12,0xBDE8,
	0x6F12,0x1040,
	0x6F12,0x1748,
	0x6F12,0x00F0,
	0x6F12,0xC5B8,
	0x6F12,0x0000,
	0x6F12,0x2000,
	0x6F12,0x9C68,
	0x6F12,0x2001,
	0x6F12,0x46B4,
	0x6F12,0x2001,
	0x6F12,0x5870,
	0x6F12,0x4000,
	0x6F12,0x9D62,
	0x6F12,0x2000,
	0x6F12,0x68C0,
	0x6F12,0x2001,
	0x6F12,0xF000,
	0x6F12,0x2001,
	0x6F12,0x45B0,
	0x6F12,0x2000,
	0x6F12,0x12F0,
	0x6F12,0x2000,
	0x6F12,0x6D34,
	0x6F12,0x2000,
	0x6F12,0xED50,
	0x6F12,0x2000,
	0x6F12,0x61F0,
	0x6F12,0x2000,
	0x6F12,0x2390,
	0x6F12,0x2000,
	0x6F12,0x3A70,
	0x6F12,0x2000,
	0x6F12,0xBA00,
	0x6F12,0x2000,
	0x6F12,0xED60,
	0x6F12,0x2000,
	0x6F12,0x9420,
	0x6F12,0x0001,
	0x6F12,0x7547,
	0x6F12,0x0001,
	0x6F12,0x7361,
	0x6F12,0x0001,
	0x6F12,0x574D,
	0x6F12,0x0002,
	0x6F12,0x432D,
	0x6F12,0x0000,
	0x6F12,0xB127,
	0x6F12,0x0001,
	0x6F12,0x7E15,
	0x6F12,0x0000,
	0x6F12,0x6D83,
	0x6F12,0x40F6,
	0x6F12,0xD91C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xCD3C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xDD3C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xD53C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x47F2,
	0x6F12,0x3D3C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x47F6,
	0x6F12,0xE95C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x48F2,
	0x6F12,0x391C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x45F2,
	0x6F12,0x4D7C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x43F6,
	0x6F12,0xB76C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x43F6,
	0x6F12,0xE96C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xA90C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xB92C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xF72C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x46F2,
	0x6F12,0xED1C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x45F6,
	0x6F12,0xE51C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x4BF2,
	0x6F12,0x770C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x40F6,
	0x6F12,0xC76C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x4AF2,
	0x6F12,0xC17C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x4AF2,
	0x6F12,0x797C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x48F2,
	0x6F12,0x691C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x41F2,
	0x6F12,0x331C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x44F2,
	0x6F12,0xE53C,
	0x6F12,0xC0F2,
	0x6F12,0x020C,
	0x6F12,0x6047,
	0x6F12,0x48F2,
	0x6F12,0x554C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x40F2,
	0x6F12,0x6D7C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x47F6,
	0x6F12,0x975C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x40F2,
	0x6F12,0x476C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x40F6,
	0x6F12,0xBF6C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x47F6,
	0x6F12,0xC55C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x43F6,
	0x6F12,0xBB1C,
	0x6F12,0xC0F2,
	0x6F12,0x010C,
	0x6F12,0x6047,
	0x6F12,0x48F2,
	0x6F12,0x551C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x4BF6,
	0x6F12,0x152C,
	0x6F12,0xC0F2,
	0x6F12,0x000C,
	0x6F12,0x6047,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0841,
	0x6F12,0x02BA,
	0x6F12,0x0000,
	0x6F12,0x003F,
	0x602A,0xF000,
	0x6F12,0x0000,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x6F12,0x0001,
	0x6F12,0x0100,
	0x6028,0x2000,
	0x602A,0x2500,
	0x6F12,0x0080,
	0x602A,0x10B8,
	0x6F12,0x0020,
	0x602A,0x1EE0,
	0x6F12,0x0078,
	0x602A,0x2870,
	0x6F12,0x0100,
	0x602A,0x250A,
	0x6F12,0x0000,
	0x602A,0x23A0,
	0x6F12,0x0001,
	0x602A,0x3022,
	0x6F12,0x1281,
	0x602A,0x32E8,
	0x6F12,0x0100,
	0x602A,0x54A2,
	0x6F12,0x0000,
	0x602A,0x120E,
	0x6F12,0x0000,
	0x602A,0x1212,
	0x6F12,0x0000,
	0x602A,0x2860,
	0x6F12,0x0001,
	0x602A,0x3220,
	0x6F12,0x0000,
	0x602A,0x1226,
	0x6F12,0x0301,
	0x602A,0x29C8,
	0x6F12,0x0000,
	0x602A,0x32EC,
	0x6F12,0x0000,
	0x602A,0x12BE,
	0x6F12,0x0101,
	0x602A,0x3034,
	0x6F12,0x049B,
	0x602A,0x1230,
	0x6F12,0x0100,
	0x602A,0x1232,
	0x6F12,0x00F0,
	0x602A,0x1236,
	0x6F12,0x01FF,
	0x602A,0x123A,
	0x6F12,0x0004,
	0x602A,0x123E,
	0x6F12,0xF45A,
	0x602A,0x1EE2,
	0x6F12,0x19CD,
	0x602A,0x115E,
	0x6F12,0x0048,
	0x602A,0x131C,
	0x6F12,0x2400,
	0x602A,0x2872,
	0x6F12,0x0001,
	0x602A,0x1314,
	0x6F12,0x0100,
	0x602A,0x20DE,
	0x6F12,0x0003,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x602A,0x2108,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x6F12,0x0022,
	0x6F12,0x0011,
	0x602A,0x1EDC,
	0x6F12,0x5008,
	0x602A,0x138E,
	0x6F12,0x13C0,
	0x602A,0x1392,
	0x6F12,0x0038,
	0x602A,0x21B6,
	0x6F12,0x0002,
	0x6F12,0x0000,
	0x602A,0x2550,
	0x6F12,0x193C,
	0x6028,0x4000,
	0x0BC0,0x0040,
	0x0FE8,0x49C1,
	0x0FEA,0x0040,
	0x0BC8,0x0001,
	0x0B0A,0x0101,
	0x0BC6,0x0000,
	0x0B06,0x0101,
	0xF446,0x000C,
	0xF448,0x0018,
	0xF450,0x0010,
	0xF44E,0x0000,
	0xF468,0xE000,
	0x6028,0x2000,
	0x602A,0x3778,
	0x6F12,0x0100,
	0x602A,0x37FC,
	0x6F12,0x0000,
	0x602A,0x4BFC,
	0x6F12,0xD2D2,
	0x6F12,0xD2D2,
	0x6F12,0xD2D2,
	0x602A,0x465C,
	0x6F12,0x1414,
	0x6F12,0x1414,
	0x6F12,0x1414,
	0x602A,0x4652,
	0x6F12,0x1023,
	0x6F12,0x2323,
	0x6F12,0x2323,
	0x6F12,0x2300,
	0x602A,0x466E,
	0x6F12,0x1313,
	0x6F12,0x1313,
	0x6F12,0x1313,
	0x602A,0x469A,
	0x6F12,0x1014,
	0x6F12,0x1414,
	0x6F12,0x1414,
	0x6F12,0x1400,
	0x602A,0x46AC,
	0x6F12,0x1013,
	0x6F12,0x1313,
	0x6F12,0x1313,
	0x6F12,0x1300,
	0x602A,0x4676,
	0x6F12,0x100A,
	0x6F12,0x0A0A,
	0x6F12,0x0A0A,
	0x6F12,0x0A00,
	0x602A,0x4688,
	0x6F12,0x101D,
	0x6F12,0x1D1D,
	0x6F12,0x1D1D,
	0x6F12,0x1D00,
	0x602A,0x4C0E,
	0x6F12,0x7878,
	0x6F12,0x7878,
	0x6F12,0x7878,
	0x602A,0x3B1E,
	0x6F12,0x008C,
	0x602A,0x4C20,
	0x6F12,0x1D1D,
	0x6F12,0x1D1D,
	0x6F12,0x1D1D,
	0x602A,0x3B12,
	0x6F12,0x0002,
	0x602A,0x3AF2,
	0x6F12,0x0002,
	0x602A,0x3AF6,
	0x6F12,0x0005,
	0x602A,0x3AFA,
	0x6F12,0x0007,
	0x602A,0x3AFE,
	0x6F12,0x0064,
	0x602A,0x3B02,
	0x6F12,0x00AF,
	0x602A,0x3B06,
	0x6F12,0x00C8,
	0x602A,0x46BE,
	0x6F12,0x10D4,
	0x6F12,0xD4D4,
	0x6F12,0xD4D4,
	0x6F12,0xD400,
	0x602A,0x46C8,
	0x6F12,0xFAFA,
	0x6F12,0xFAFA,
	0x6F12,0xFAFA,
	0x602A,0x3B2E,
	0x6F12,0x0008,
	0x602A,0x3B32,
	0x6F12,0x0070,
	0x602A,0x4C28,
	0x6F12,0x1033,
	0x6F12,0x3333,
	0x6F12,0x3333,
	0x6F12,0x3300,
	0x602A,0x4C32,
	0x6F12,0x1919,
	0x6F12,0x1919,
	0x6F12,0x1919,
	0x602A,0x4C3A,
	0x6F12,0x10CC,
	0x6F12,0xCCCC,
	0x6F12,0xCCCC,
	0x6F12,0xCC00,
	0x602A,0x4C44,
	0x6F12,0x3333,
	0x6F12,0x3333,
	0x6F12,0x3333,
	0x602A,0x4C4C,
	0x6F12,0x1066,
	0x6F12,0x6666,
	0x6F12,0x6666,
	0x6F12,0x6600,
	0x602A,0x4C56,
	0x6F12,0x2222,
	0x6F12,0x2222,
	0x6F12,0x2222,
	0x602A,0x3E06,
	0x6F12,0x0000,
	0x602A,0x3E0A,
	0x6F12,0x0000,
	0x602A,0x3E2E,
	0x6F12,0x0060,
	0x602A,0x37FE,
	0x6F12,0x0001,
	0x6F12,0x0001,
	0x6F12,0x0001,
	0x6F12,0x0001,
	0x602A,0x3E0E,
	0x6F12,0x0019,
	0x602A,0x3E12,
	0x6F12,0x00FE,
	0x602A,0x3E16,
	0x6F12,0x0019,
	0x602A,0x3E1A,
	0x6F12,0x00FE,
	0x602A,0x3E1E,
	0x6F12,0x001E,
	0x602A,0x3E22,
	0x6F12,0x00FF,
	0x602A,0x3E26,
	0x6F12,0x0014,
	0x602A,0x3E2A,
	0x6F12,0x00DA,
	0x602A,0x3CB2,
	0x6F12,0x0000,
	0x602A,0x3BA2,
	0x6F12,0x0000,
	0x602A,0x4C5E,
	0x6F12,0x4078,
	0x6F12,0x785E,
	0x6F12,0x4236,
	0x6F12,0x3601,
	0x602A,0x4C68,
	0x6F12,0x7878,
	0x6F12,0x5E42,
	0x6F12,0x3636,
	0x602A,0x4C70,
	0x6F12,0x405A,
	0x6F12,0x5A78,
	0x6F12,0x96B4,
	0x6F12,0xB401,
	0x602A,0x4C7A,
	0x6F12,0x6464,
	0x6F12,0x85A7,
	0x602A,0x4C82,
	0x6F12,0x4053,
	0x6F12,0x5370,
	0x6F12,0x8BA7,
	0x6F12,0xA701,
	0x602A,0x4C8C,
	0x6F12,0x5353,
	0x6F12,0x708B,
	0x6F12,0xA7A7,
	0x602A,0x4C94,
	0x6F12,0x4064,
	0x6F12,0x6486,
	0x6F12,0xA7C8,
	0x6F12,0xC801,
	0x602A,0x4C9E,
	0x6F12,0x1414,
	0x6F12,0x1B21,
	0x6F12,0x2828,
	0x602A,0x4CA6,
	0x6F12,0x4014,
	0x6F12,0x141B,
	0x6F12,0x2128,
	0x6F12,0x2801,
	0x602A,0x4CB0,
	0x6F12,0x1B1B,
	0x6F12,0x232D,
	0x6F12,0x3636,
	0x602A,0x4CB8,
	0x6F12,0x403C,
	0x6F12,0x3C50,
	0x6F12,0x6478,
	0x6F12,0x7801,
	0x602A,0x4CC2,
	0x6F12,0x3C3C,
	0x6F12,0x5064,
	0x6F12,0x7878,
	0x602A,0x3DA6,
	0x6F12,0x0035,
	0x602A,0x3DAA,
	0x6F12,0x0028,
	0x602A,0x3DB0,
	0x6F12,0x01AB,
	0x6F12,0x0001,
	0x6F12,0x01AC,
	0x6F12,0x0050,
	0x6F12,0x01AD,
	0x6F12,0x0064,
	0x6F12,0x01AE,
	0x6F12,0x0064,
	0x6F12,0x01AF,
	0x6F12,0x00C8,
	0x6F12,0x01B0,
	0x6F12,0x00C8,
	0x602A,0x3DD4,
	0x6F12,0x01B4,
	0x6F12,0x0032,
	0x6F12,0x01B5,
	0x6F12,0x0050,
	0x6F12,0x01B6,
	0x6F12,0x0050,
	0x6F12,0x01B7,
	0x6F12,0x00C8,
	0x6F12,0x01B8,
	0x6F12,0x00C8,
	0x6F12,0x01B9,
	0x6F12,0x0081,
	0x6028,0x4000,
	0xF452,0x000D,
};

static void sensor_init(void)
{
	LOG_INF("sensor_init() E\n");

	write_cmos_sensor(0x0100,0x0000);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0000,0x0010);
	write_cmos_sensor(0x0000,0x0841);
	write_cmos_sensor(0x6010,0x0001);
	mdelay(24);

	table_write_cmos_sensor(addr_data_pair_init_gd1,
		sizeof(addr_data_pair_init_gd1) / sizeof(kal_uint16));

}

static kal_uint16 addr_data_pair_pre_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};


static void preview_setting(void)
{
	LOG_INF("preview_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_pre_gd1,
			sizeof(addr_data_pair_pre_gd1) / sizeof(kal_uint16));



}

static kal_uint16 addr_data_pair_cap_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_cap_gd1,
			sizeof(addr_data_pair_cap_gd1) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_video_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting() E! currefps:%d\n", currefps);


	table_write_cmos_sensor(addr_data_pair_video_gd1,
		sizeof(addr_data_pair_video_gd1) / sizeof(kal_uint16));



}

static kal_uint16 addr_data_pair_hs_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};

static void hs_video_setting(void)
{
	LOG_INF("hs_video_setting() E\n");






	table_write_cmos_sensor(addr_data_pair_hs_gd1,
			sizeof(addr_data_pair_hs_gd1) / sizeof(kal_uint16));


}

static kal_uint16 addr_data_pair_slim_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting() E\n");




	table_write_cmos_sensor(addr_data_pair_slim_gd1,
		sizeof(addr_data_pair_slim_gd1) / sizeof(kal_uint16));

}

#if STEREO_CUSTOM1_30FPS
static kal_uint16 addr_data_pair_custom1_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};
#else
static kal_uint16 addr_data_pair_custom1_gd1[] = {
	0x6028,0x4000,
	0x6214,0xF9F0,
	0x6218,0xE150,
	0x6242,0x0E00,
	0x6028,0x2000,
	0x602A,0x12F2,
	0x6F12,0x0D10,
	0x6F12,0x0A18,
	0x6F12,0x19B0,
	0x6F12,0x1350,
	0x602A,0x1EB6,
	0x6F12,0x0206,
	0x602A,0x3770,
	0x6F12,0x0100,
	0x602A,0x1EB8,
	0x6F12,0x0301,
	0x602A,0x131E,
	0x6F12,0x0100,
	0x602A,0x3DEA,
	0x6F12,0x0081,
	0x602A,0x11A6,
	0x6F12,0x0000,
	0x6F12,0x0004,
	0x602A,0x11AE,
	0x6F12,0x0003,
	0x602A,0x13FC,
	0x6F12,0x0044,
	0x6F12,0x0064,
	0x6F12,0x0044,
	0x602A,0x3302,
	0x6F12,0x0100,
	0x6F12,0x0100,
	0x6F12,0x0001,
	0x602A,0x27D2,
	0x6F12,0x0203,
	0x602A,0x1EC8,
	0x6F12,0x0503,
	0x6F12,0x0504,
	0x602A,0x1ED2,
	0x6F12,0x080F,
	0x602A,0x1ED6,
	0x6F12,0x0307,
	0x602A,0x123C,
	0x6F12,0x0009,
	0x602A,0x21BE,
	0x6F12,0x04D2,
	0x6F12,0x41A6,
	0x602A,0x145C,
	0x6F12,0x0035,
	0x6F12,0x0049,
	0x6F12,0x0035,
	0x602A,0x140E,
	0x6F12,0x0000,
	0x6F12,0x0001,
	0x6F12,0x0000,
	0x6028,0x4000,
	0xF466,0x0E0D,
	0x0344,0x0008,
	0x0346,0x0000,
	0x0348,0x19A7,
	0x034A,0x134E,
	0x034C,0x0CD0,
	0x034E,0x09A0,
	0x0350,0x0000,
	0x0352,0x0004,
	0x0900,0x0112,
	0x0380,0x0001,
	0x0382,0x0001,
	0x0384,0x0002,
	0x0386,0x0002,
	0x0400,0x2010,
	0x0404,0x1000,
	0x0402,0x1010,
	0x0114,0x0300,
	0x0116,0x3000,
	0x0110,0x1002,
	0x011C,0x0100,
	0x0136,0x1800,
	0x0300,0x0002,
	0x0302,0x0003,
	0x0304,0x0004,
	0x0306,0x011E,
	0x0308,0x0008,
	0x030A,0x0002,
	0x030C,0x0000,
	0x030E,0x0004,
	0x0310,0x0120,
	0x0312,0x0002,
	0x0340,0x0A40,
	0x0342,0x38C0,
	0x0202,0x0100,
	0x0200,0x0100,
	0x022C,0x0100,
	0x0226,0x0100,
	0x021E,0x0000,
	0x6028,0x2000,
	0x602A,0x3020,
	0x6F12,0x0000,
	0x6028,0x4000,
	0x0B00,0x0080,
	0x0B08,0x0000,
	0x0D00,0x0101,
	0x0D02,0x0001,
	0x0D04,0x0000,
};
#endif


static void custom1_setting(void)
{
#if STEREO_CUSTOM1_30FPS
	LOG_INF("custom1_setting() 6 M*30 fps E!\n");
#else
	LOG_INF("custom1_setting() 6 M*24 fps E!\n");
#endif
	LOG_INF("custom1_setting() use custom1\n");
	table_write_cmos_sensor(addr_data_pair_custom1_gd1,
			sizeof(addr_data_pair_custom1_gd1) / sizeof(kal_uint16));


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
 static int get_board_id(void)
{
	// static int board_id = 0xff;
	// int bat_id_voltage = 0;
	// int res = 0;
    // int data[4] = {0,0,0,0};
	// int rawdata =0;

	// if(board_id == 0xff)
	// {
		// res = IMM_GetOneChannelValue(AUXADC_CAMERA_VOLTAGE_CHANNEL,data,&rawdata);
		// bat_id_voltage = data[0] * 1000 + data[1] * 10;
		// printk("[s5kgd1]MIN_VOLTAGE = %d,MAX_VOLTAGE = %d\n",MIN_VOLTAGE,MAX_VOLTAGE);
		// if(bat_id_voltage>=MIN_VOLTAGE && bat_id_voltage <= MAX_VOLTAGE)
		// {
			// CheckGmsFlag = 0;
		// }
		// else
		// {
			// CheckGmsFlag = 1;
		// }
	// }
	return 0;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5;
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			if (*sensor_id == imgsensor_info.sensor_id)
			{
				get_board_id();
				printk("[s5kgd1]CheckGmsFlag = %d\n",CheckGmsFlag);
				if(CheckGmsFlag == 0)
				{
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x, *sensor_id = 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id, *sensor_id);
				return ERROR_NONE;
				}
				else
				{
				*sensor_id = 0xFFFFFFFF;
				return ERROR_SENSOR_CONNECT_FAIL;
				}
			}
			LOG_INF("Read sensor id fail, id: 0x%x, *sensor_id = 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {

		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
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

	LOG_INF("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = (
		(read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			LOG_INF("Read sensor id fail, id: 0x%x\n",
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
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}



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
	LOG_INF("E\n");



	return ERROR_NONE;
}


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
	LOG_INF("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

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
	LOG_INF("capture E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap.max_framerate / 10);
		}

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video E\n");

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

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;

	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("slim_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;

	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E cur fps: %d\n", imgsensor.current_fps);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
	set_mirror_flip(imgsensor.mirror);


    return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("get_resolution E\n");
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

	sensor_resolution->SensorCustom1Width  =
		imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height     =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("get_info -> scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;


	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;


	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

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

	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;


	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;


	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;


	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;

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
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;

	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
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
          custom1(image_window, sensor_config_data);
        break;

	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}



static kal_uint32 set_video_mode(UINT16 framerate)
{


	if (framerate == 0)

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

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;
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
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		frame_length = imgsensor_info.cap1.pclk
			/ framerate * 10 / imgsensor_info.cap1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap1.framelength)
		    ? (frame_length - imgsensor_info.cap1.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		frame_length = imgsensor_info.cap2.pclk
			/ framerate * 10 / imgsensor_info.cap2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		      (frame_length > imgsensor_info.cap2.framelength)
		    ? (frame_length - imgsensor_info.cap2.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap2.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
	}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			/ framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	default:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

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
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
		LOG_INF("custom1 *framerate = %d\n", *framerate);
		break;

	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {


         write_cmos_sensor(0x3202, 0x0080);
         write_cmos_sensor(0x3204, 0x0080);
         write_cmos_sensor(0x3206, 0x0080);
         write_cmos_sensor(0x3208, 0x0080);
         write_cmos_sensor(0x3232, 0x0000);
         write_cmos_sensor(0x3234, 0x0000);
         write_cmos_sensor(0x32a0, 0x0100);
         write_cmos_sensor(0x3300, 0x0001);
         write_cmos_sensor(0x3400, 0x0001);
         write_cmos_sensor(0x3402, 0x4e00);
         write_cmos_sensor(0x3268, 0x0000);
         write_cmos_sensor(0x0600, 0x0002);
	} else {


         write_cmos_sensor(0x3202, 0x0000);
         write_cmos_sensor(0x3204, 0x0000);
         write_cmos_sensor(0x3206, 0x0000);
         write_cmos_sensor(0x3208, 0x0000);
         write_cmos_sensor(0x3232, 0x0000);
         write_cmos_sensor(0x3234, 0x0000);
         write_cmos_sensor(0x32a0, 0x0000);
         write_cmos_sensor(0x3300, 0x0000);
         write_cmos_sensor(0x3400, 0x0000);
         write_cmos_sensor(0x3402, 0x0000);
         write_cmos_sensor(0x3268, 0x0000);
         write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	/*pr_info("temp_c(%d), read_reg(%d), enable %d\n",
	 *	temperature_convert, temperature, read_cmos_sensor_8(0x0138));
	 */

	return temperature_convert;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
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

		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
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
		set_auto_flicker_mode((BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
		break;


	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

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
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
 * (UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));

		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80))*
			imgsensor_info.custom1.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
//chenpeng add
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data)
		{
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data)
		{
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.cap.framelength << 16) + imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.normal_video.framelength << 16) + imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.hs_video.framelength << 16) + imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.slim_video.framelength << 16) + imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.pre.framelength << 16) + imgsensor_info.pre.linelength;
			break;
		}
		break;
		//chenpeng add end
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KGD1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{

	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
