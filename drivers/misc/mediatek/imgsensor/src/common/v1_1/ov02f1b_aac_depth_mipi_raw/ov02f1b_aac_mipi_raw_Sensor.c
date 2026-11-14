/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV02F1Bmipiraw_Sensor.c
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
 *----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "ov02f1b_aac_mipi_raw_Sensor.h"
#include "../imgsensor_i2c.h"
/*********************Modify Following Strings for Debug********************/
#define PFX "OV02F1B_aac_camera_sensor"
#define LOG_1 LOG_INF("OV02F1B,MIPI 1LANE\n")
/***************************   Modify end    *******************************/

#define OV02F1B_AAC_VENDOR_ID 0x10

#define LOG_INF(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV02F1B_AAC_DEPTH_SENSOR_ID,

	.checksum_value = 0x64d5ee2e,

	.pre = {
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.hs_video = {		/*slow motion */
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.slim_video = {		/*VT Call */
		.pclk = 16500000,
		.linelength = 430,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 300,
		},
	.custom1 = {
		.pclk = 16500000,
		.linelength  = 430,
		.framelength = 1598,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 66000000,
		.max_framerate = 240,
	},
	.margin = 7,		//sensor framelength & shutter margin
	.min_shutter = 4,	//min shutter
	.min_gain = 64, /*1x gain*/
	.max_gain = 960, /*15.5x gain*/
	.min_gain_iso = 50,
	.gain_step = 1,
	.gain_type = 1,/*to be modify,no gain table for gc*/
	.max_frame_length = 0x7ff7,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 6,	/* support sensor mode num */
	.frame_time_delay_frame = 2,

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x78, 0x7a,0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x10,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x78,	/* record current sensor's i2c write id */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/* Preview */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/* capture */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/* video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/* HS video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/* slim video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0000, 0000, 1600, 1200, 0, 0, 1600, 1200},	/*custom1*/
};

static kal_uint32 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint32 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1,
		    imgsensor.i2c_write_id);

	return get_byte;

}


static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xff), (char)(para & 0xff) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	/*O7 code for HQ-404750 by chenxiaoyong at 20240731 start*/
	if(imgsensor.frame_length%2 != 0){
		imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;
	}

	LOG_INF("imgsensor.frame_length = %d\n", imgsensor.frame_length);

	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x14, ((imgsensor.frame_length - 1220) & 0x7F00) >> 8);
	write_cmos_sensor(0x15, (imgsensor.frame_length - 1220) & 0xFF);
	write_cmos_sensor(0x25, imgsensor.line_length >> 8);
	write_cmos_sensor(0x26, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0xfe, 0x02);
	/*O7 code for HQ-404750 by chenxiaoyong at 20240731 end*/

}				/*      set_dummy  */

static kal_uint32 return_sensor_id(void)
{
    write_cmos_sensor(0xfd, 0x00);

    return ((read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
		framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;
	}

	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}				/*      set_max_framerate  */


/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	LOG_INF("shutter =%d\n", shutter);

	if (!shutter)
		shutter = 1;	/*avoid 0 */

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter =
	    (shutter >
	     (imgsensor_info.max_frame_length -
	      imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
					 imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			
        write_cmos_sensor(0xfd, 0x01);
	    write_cmos_sensor(0x14, (imgsensor.frame_length - 0x398) >> 8);
	    write_cmos_sensor(0x15, (imgsensor.frame_length - 0x398) & 0xFF);
        write_cmos_sensor(0xfe, 0x02);
		
		
		}
	} else {
		/* Extend frame length */
       	
        write_cmos_sensor(0xfd, 0x01);
	    write_cmos_sensor(0x14, (imgsensor.frame_length - 0x398) >> 8);
	    write_cmos_sensor(0x15, (imgsensor.frame_length - 0x398) & 0xFF);
        write_cmos_sensor(0xfe, 0x02);
	}

	/* Update Shutter */
	 write_cmos_sensor(0xfd, 0x01);
	 write_cmos_sensor(0x0e, (shutter >> 8) & 0xFF);
	 write_cmos_sensor(0x0f, shutter  & 0xFF);
	 write_cmos_sensor(0xfe, 0x02);
	LOG_INF("Exit! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

	/* LOG_INF("frame_length = %d ", frame_length); */

}				/*      set_shutter */

static void set_shutter_frame_length(
		kal_uint16 shutter, kal_uint16 frame_length, kal_bool auto_extend_en)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    spin_lock(&imgsensor_drv_lock);
    /*Change frame time*/
    if (frame_length > 1) {
	dummy_line = frame_length - imgsensor.frame_length;
    }
    imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin) {
	imgsensor.frame_length = shutter + imgsensor_info.margin;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
	shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);
    }

    //imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;

    if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			/* Extend frame length*/
			write_cmos_sensor(0xfd, 0x01);
			write_cmos_sensor(0x14, ((imgsensor.frame_length-0x4c4) & 0x7F00) >> 8);
			write_cmos_sensor(0x15, (imgsensor.frame_length-0x4c4) & 0xFF);
			write_cmos_sensor(0xfe, 0x02);
		}
    } else {
	/* Extend frame length*/
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x14, ((imgsensor.frame_length-0x4c4) & 0x7F00) >> 8);
	write_cmos_sensor(0x15, (imgsensor.frame_length-0x4c4) & 0xFF);
	write_cmos_sensor(0xfe, 0x02);
    }

    /* Update Shutter*/
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x0f, (shutter) & 0xFF);
    write_cmos_sensor(0x0e, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0xfe, 0x02);
    LOG_INF("Add for N3D! shutterlzl =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{

	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain >> 2;
	reg_gain &= 0x0FF;

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

	/*
	 * sensor gain 1x = 128
	 * max gain = 0x7ff = 15.992x <16x
	 * here we just use 0x3508 analog gain 1 bit[3:2].
	 * 16x~32x should use 0x3508 analog gain 0 bit[1:0]
	 */

	if (gain < BASEGAIN || gain >= 15.5 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain >= 15.5 * BASEGAIN)
			gain = 15.5* BASEGAIN;
	}

	reg_gain = gain2reg(gain);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

  write_cmos_sensor(0xfd, 0x01);
  write_cmos_sensor(0x22, reg_gain & 0xff);
  write_cmos_sensor(0xfe, 0x02);//fresh

	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
				    kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);

}



#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	printk("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x3820,
			((read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		write_cmos_sensor(0x3821,
			((read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		break;
	default:
		printk("Error image_mirror setting\n");
	}

}
#endif

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
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function*/
}				/*      night_mode      */

static void sensor_init(void)
{
	LOG_INF("E\n");

	/*
	 * @@Initial - MIPI 1-Lane 1600x1200 10-bit 30fps
	 * 100 99 1600 1200
	 */
	/* Reset */
write_cmos_sensor(0xfc,0x01);
write_cmos_sensor(0xfd,0x00);
write_cmos_sensor(0x24,0x02);
write_cmos_sensor(0x25,0x13);
write_cmos_sensor(0x26,0x01);
write_cmos_sensor(0x29,0x03);
write_cmos_sensor(0x2a,0x34);
write_cmos_sensor(0x27,0x00);
write_cmos_sensor(0x28,0x00);
write_cmos_sensor(0x4f,0x06);
write_cmos_sensor(0x50,0x40);
write_cmos_sensor(0x51,0x04);
write_cmos_sensor(0x52,0xb0);
write_cmos_sensor(0x5b,0x08);
write_cmos_sensor(0x74,0x01);
write_cmos_sensor(0x33,0x00);
write_cmos_sensor(0x35,0x03);
write_cmos_sensor(0x4a,0x0a);
write_cmos_sensor(0x46,0x02);
write_cmos_sensor(0x3a,0x05);
write_cmos_sensor(0x3b,0x02);
write_cmos_sensor(0x3c,0x01);
write_cmos_sensor(0x3e,0x01);
write_cmos_sensor(0xfd,0x01);
write_cmos_sensor(0x30,0x01);
/*O7 code for HQ-421585 by liuzhhui at 2024-09-19 start*/
write_cmos_sensor(0x31,0x0c);
/*O7 code for HQ-421585 by liuzhhui at 2024-09-19 end*/
write_cmos_sensor(0x32,0x04);
write_cmos_sensor(0x33,0x02);
write_cmos_sensor(0x34,0x05);
write_cmos_sensor(0x35,0x60);
write_cmos_sensor(0x36,0x01);
write_cmos_sensor(0x37,0x07);
write_cmos_sensor(0x38,0x01);
write_cmos_sensor(0x39,0x01);
write_cmos_sensor(0x3a,0x01);
write_cmos_sensor(0x3b,0x01);
write_cmos_sensor(0x3c,0x01);
write_cmos_sensor(0x3d,0x01);
write_cmos_sensor(0x3e,0x01);
write_cmos_sensor(0x3f,0x00);
write_cmos_sensor(0x40,0x01);
write_cmos_sensor(0x41,0x01);
write_cmos_sensor(0x42,0x0e);
write_cmos_sensor(0x43,0x1a);
write_cmos_sensor(0x44,0x01);
write_cmos_sensor(0x45,0x73);
write_cmos_sensor(0x46,0x73);
write_cmos_sensor(0x47,0x73);
write_cmos_sensor(0x48,0x73);
write_cmos_sensor(0x49,0x01);
write_cmos_sensor(0x4a,0x75);
write_cmos_sensor(0x4b,0x75);
write_cmos_sensor(0x4c,0x75);
write_cmos_sensor(0x4d,0x75);
write_cmos_sensor(0x4e,0x05);
write_cmos_sensor(0x4f,0x04);
write_cmos_sensor(0x50,0x01);
write_cmos_sensor(0x51,0x01);
write_cmos_sensor(0x52,0x0a);
write_cmos_sensor(0x53,0x01);
write_cmos_sensor(0x54,0x10);
write_cmos_sensor(0x55,0x04);
write_cmos_sensor(0x19,0x25);
write_cmos_sensor(0x1b,0x0b);
write_cmos_sensor(0x1c,0x09);
write_cmos_sensor(0x56,0x86);
write_cmos_sensor(0x57,0x0c);
write_cmos_sensor(0x58,0x28);
write_cmos_sensor(0x59,0x17);
write_cmos_sensor(0x5a,0x0c);
write_cmos_sensor(0x5b,0x8c);
write_cmos_sensor(0x5c,0x02);
write_cmos_sensor(0x5d,0x0a);
write_cmos_sensor(0x5e,0x01);
write_cmos_sensor(0x5f,0x04);
write_cmos_sensor(0x60,0x01);
write_cmos_sensor(0x61,0x04);
write_cmos_sensor(0x62,0x04);
write_cmos_sensor(0x63,0x03);
write_cmos_sensor(0x64,0x01);
write_cmos_sensor(0x65,0x00);
write_cmos_sensor(0x66,0x2a);
write_cmos_sensor(0x16,0x00);
write_cmos_sensor(0x17,0x42);
write_cmos_sensor(0x6e,0x01);
write_cmos_sensor(0x98,0x00);
write_cmos_sensor(0x0d,0x44);
write_cmos_sensor(0x6f,0x22);
write_cmos_sensor(0x68,0x90);
/*O7 code for HQ-421585 by liuzhhui at 2024-09-19 start*/
write_cmos_sensor(0x69,0xcd);
/*O7 code for HQ-421585 by liuzhhui at 2024-09-19 end*/
write_cmos_sensor(0x6a,0x0f);
write_cmos_sensor(0x6b,0x00);
write_cmos_sensor(0x7c,0x05);
write_cmos_sensor(0x7d,0x05);
write_cmos_sensor(0x7e,0x05);
write_cmos_sensor(0x7f,0x05);
write_cmos_sensor(0x83,0x14);
write_cmos_sensor(0x84,0x14);
write_cmos_sensor(0x85,0x14);
write_cmos_sensor(0x86,0x14);
write_cmos_sensor(0x89,0x18);
write_cmos_sensor(0x8a,0x18);
write_cmos_sensor(0x8b,0x1b);
write_cmos_sensor(0x8c,0x1c);
write_cmos_sensor(0x8d,0x1e);
write_cmos_sensor(0x88,0x0f);
write_cmos_sensor(0x94,0x02);
write_cmos_sensor(0x98,0xd1);
}/*sensor_init  */

static void preview_setting(void)
{
	LOG_INF("E\n");
	/*
	 * @@Initial - MIPI 1-Lane 1600x1200 10-bit 30fps
	 * 100 99 1600 1200
	 */
write_cmos_sensor(0xfd,0x01);
write_cmos_sensor(0x14,0x00);
write_cmos_sensor(0x15,0x38);
write_cmos_sensor(0x02,0x00);
write_cmos_sensor(0x03,0x70);
write_cmos_sensor(0x04,0x00);
write_cmos_sensor(0x05,0x10);
write_cmos_sensor(0x06,0x03);
write_cmos_sensor(0x07,0x20);
write_cmos_sensor(0x08,0x04);
write_cmos_sensor(0x09,0xb0);
write_cmos_sensor(0xfd,0x01);
write_cmos_sensor(0x18,0x00);
write_cmos_sensor(0x0e,0x00);
write_cmos_sensor(0x0f,0x05);
write_cmos_sensor(0x21,0x00);
write_cmos_sensor(0x22,0xf8);
write_cmos_sensor(0x20,0x03);
write_cmos_sensor(0x23,0x04);
write_cmos_sensor(0xfd,0x03);
write_cmos_sensor(0xae,0x0d);
write_cmos_sensor(0x88,0x7b);
write_cmos_sensor(0x89,0x7c);
write_cmos_sensor(0xb4,0x05);
write_cmos_sensor(0x81,0x00);
write_cmos_sensor(0x8c,0x40);
write_cmos_sensor(0x8e,0x40);
write_cmos_sensor(0x90,0x40);
write_cmos_sensor(0x92,0x40);
write_cmos_sensor(0x97,0x60);
write_cmos_sensor(0x98,0x60);
write_cmos_sensor(0x99,0x60);
write_cmos_sensor(0x9a,0x60);
write_cmos_sensor(0xbd,0x02);
write_cmos_sensor(0x9b,0x40);
write_cmos_sensor(0xac,0x40);
write_cmos_sensor(0xaf,0x08);
write_cmos_sensor(0xb0,0x00);
write_cmos_sensor(0xc2,0x01);
write_cmos_sensor(0xfe,0x02);
write_cmos_sensor(0xfb,0x01);
	mdelay(5);
}	/*      preview_setting  */

static void capture_setting(void)
{
	printk("E\n");

    preview_setting();
}	/*capture_setting  */

static void normal_video_setting(void)
{
    preview_setting();
}


static void hs_video_setting(void)
{
	LOG_INF("E\n");

    preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

static void custom1_setting(void)
{
    LOG_INF("custom1_setting E!\n");

write_cmos_sensor(0xfd,0x01);
write_cmos_sensor(0x14,0x01);
write_cmos_sensor(0x15,0x7a);
write_cmos_sensor(0x02,0x00);
write_cmos_sensor(0x03,0x70);
write_cmos_sensor(0x04,0x00);
write_cmos_sensor(0x05,0x10);
write_cmos_sensor(0x06,0x03);
write_cmos_sensor(0x07,0x20);
write_cmos_sensor(0x08,0x04);
write_cmos_sensor(0x09,0xb0);
write_cmos_sensor(0xfd,0x01);
write_cmos_sensor(0x18,0x00);
write_cmos_sensor(0x0e,0x00);
write_cmos_sensor(0x0f,0x05);
write_cmos_sensor(0x21,0x00);
write_cmos_sensor(0x22,0xf8);
write_cmos_sensor(0x20,0x03);
write_cmos_sensor(0x23,0x04);
write_cmos_sensor(0xfd,0x03);
write_cmos_sensor(0xae,0x0d);
write_cmos_sensor(0x88,0x7b);
write_cmos_sensor(0x89,0x7c);
write_cmos_sensor(0xb4,0x05);
write_cmos_sensor(0x81,0x00);
write_cmos_sensor(0x8c,0x40);
write_cmos_sensor(0x8e,0x40);
write_cmos_sensor(0x90,0x40);
write_cmos_sensor(0x92,0x40);
write_cmos_sensor(0x97,0x60);
write_cmos_sensor(0x98,0x60);
write_cmos_sensor(0x99,0x60);
write_cmos_sensor(0x9a,0x60);
write_cmos_sensor(0xbd,0x02);
write_cmos_sensor(0x9b,0x40);
write_cmos_sensor(0xac,0x40);
write_cmos_sensor(0xaf,0x08);
write_cmos_sensor(0xb0,0x00);
write_cmos_sensor(0xc2,0x01);
write_cmos_sensor(0xfe,0x02);
write_cmos_sensor(0xfb,0x01);
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
  static char otpData[16]={0};
  static kal_int8 ov02f1b_read_general_info(int *addr, kal_uint16 addr_len, kal_uint8 *result)
  {
  	int i = 0;
  	if (addr_len == 0 || result == NULL) {
  		LOG_INF("Error info\n");
  		return -1;
  	}
  	write_cmos_sensor(0xfd, 0x06);
  	write_cmos_sensor(0x21, 0x00);
  	write_cmos_sensor(0x2f, 0x01);

  	for (i = 0; i < addr_len; i++) {
  		result[i] = read_cmos_sensor(addr[i]);
  	}
  	return 0;
  }

  unsigned int ov02f1b_read_otp(void)
  {
  	int ret = 0;
	kal_uint8 i;
	/*O7 code for HQ-390105 by chenxiaoyong at 20240705 start*/
	int fuseid_addr[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0X0D,0X0E,0X0F};
	ret = ov02f1b_read_general_info(fuseid_addr, ARRAY_SIZE(fuseid_addr), otpData);
	/*O7 code for HQ-390105 by chenxiaoyong at 20240705 end*/
  	if (ret != 0) {
  		LOG_INF("read fuseid failed\n");
  		return -1;
  	}
     for(i = 0; i < ARRAY_SIZE(otpData); i++){
		LOG_INF("read otp data 0x%x=0x%x",fuseid_addr[i],otpData[i]);
	}
  	return ret;
  }

unsigned int ov02f1b_read_otp_info(struct i2c_client *client,
  		 unsigned int addr, unsigned char *data, unsigned int size)
  {
  	int num = ARRAY_SIZE(otpData);
	int index = 0;
	LOG_INF("addr:0x%x ,size:%d",addr, size);
	for(;index<num ;index++){
		data[index] = otpData[index];
	}
  	return num;
}
EXPORT_SYMBOL(ov02f1b_read_otp_info);

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	/*O7 code for HQ-390105 by chenxiaoyong at 20240705 start*/
	kal_uint8 retry_total = 3;
	/*O7 code for HQ-390105 by chenxiaoyong at 20240705 end*/
	kal_uint8 retry_cnt = retry_total;
	static int flag = 0;
	int ret = 0;
	if(flag == 0)
	{
		ret  = ov02f1b_read_otp();
	    if (ret != 0) {
		    pr_err("Read ov02f1b aac otp info failed");
		    *sensor_id = 0xFFFFFFFF;
		    /*O7 code for HQ-390105 by chenxiaoyong at 20240705 start*/
		    return ERROR_SENSOR_CONNECT_FAIL;
		    /*O7 code for HQ-390105 by chenxiaoyong at 20240705 end*/
		}
		flag = 1;
	}

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			/*O7 code for HQ-390105 by chenxiaoyong at 20240705 start*/
			LOG_INF("[ov02f1b_camera_sensor] vendor_id = 0x%2x", otpData[15]);
			if (*sensor_id == imgsensor_info.sensor_id && otpData[15] == OV02F1B_AAC_VENDOR_ID) {
			/*O7 code for HQ-390105 by chenxiaoyong at 20240705 end*/
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry_cnt--;
		} while (retry_cnt > 0);
		i++;
		retry_cnt = retry_total;
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
  	kal_uint32 sensor_id = 0;
	LOG_1;
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();

			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
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
	imgsensor.gain = 0x10;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*      open  */



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

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


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
	LOG_INF("preview setting E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	//set_mirror_flip(IMAGE_H_MIRROR);
	return ERROR_NONE;
}				/*      preview   */

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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			     imgsensor.current_fps,
			     imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	//set_mirror_flip(IMAGE_HV_MIRROR);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 600; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 1200; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}				/*      slim_video       */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("Custom1 E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
   // set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
} /*  Custom1   */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	LOG_INF("E\n");
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
		
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
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
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */

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
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);

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

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/*L19 code for 161200 by zhangmeng at 2021.11.24 start*/
static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
		scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.normal_video.framelength) ?
			(frame_length -
			imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
			    imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap1.framelength) ?
				(frame_length -
				imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate / 10);
			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 /
			    imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.cap.framelength) ?
				(frame_length -
				imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.hs_video.framelength) ?
			(frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.slim_video.framelength) ?
			(frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length >
		     imgsensor_info.custom1.framelength) ? (frame_length -
							       imgsensor_info.
							       custom1.
							       framelength) : 0;
		if (imgsensor.dummy_line < 0) {
			imgsensor.dummy_line = 0;
		}
		imgsensor.frame_length =
		   imgsensor_info.custom1.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
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
/*L19 code for 161200 by zhangmeng at 2021.11.24 end*/


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
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
		write_cmos_sensor(0xfd, 0x03);
		write_cmos_sensor(0x81, 0x01);
	}
	else {
		write_cmos_sensor(0xfd, 0x03);
		write_cmos_sensor(0x81, 0x00);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);

	if (enable){
		write_cmos_sensor(0xfd, 0x03);
		write_cmos_sensor(0xc2, 0x01);
		} else {
		write_cmos_sensor(0xfd, 0x03);
		/*O7 code for HQ-404750 by chenxiaoyong at 20240731 start*/
		write_cmos_sensor(0xc2, 0x00);
		/*O7 code for HQ-404750 by chenxiaoyong at 20240731 end*/
		}	
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
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
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(feature_data + 2) = 2;
            break;
        default:
            *(feature_data + 2) = 1;
            break;
        }
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
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
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate =
				    imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate =
				    imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate =
				    imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		    read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
			*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL) * feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t) (*(feature_data + 1));

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
			LOG_INF("lch MSDK_SCENARIO_ID_CUSTOM1 \n");
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
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
	#if 0
	case SENSOR_FEATURE_GET_CUSTOM_INFO:
	    printk("SENSOR_FEATURE_GET_CUSTOM_INFO information type:%lld  OV02F1B_OTP_ERROR_CODE:%d \n", *feature_data,OV02F1B_OTP_ERROR_CODE);
		switch (*feature_data) {
			case 0:    //info type: otp state
			printk("*feature_para_len = %d, sizeof(MUINT32)*13 + 2 =%ld, \n", *feature_para_len, sizeof(MUINT32)*13 + 2);
			if (*feature_para_len >= sizeof(MUINT32)*13 + 2) {
			    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = OV02F1B_OTP_ERROR_CODE;//otp_state
				memcpy( feature_data+2, sn_inf_main_ov02f1b, sizeof(MUINT32)*13); 
				memcpy( feature_data+10, material_inf_main_ov02f1b, sizeof(MUINT32)*4); 
				#if 0
						for (i = 0 ; i<12 ; i++ ){
						printk("sn_inf_main_ov02f1b[%d]= 0x%x\n", i, sn_inf_main_ov02f1b[i]);
						}
				#endif
			}
				break;
			}
			break;
	#endif
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
/*L19 code for HQ-161200 by WangChenXu at 2021/11/17 start*/
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) *feature_data,
	            (UINT16) *(feature_data + 1), (BOOL) (*(feature_data + 2)));
		break;
/*L19 code for HQ-161200 by WangChenXu at 2021/11/17 end*/
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV02F1B_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      OV02F1B_MIPI_RAW_SensorInit      */
