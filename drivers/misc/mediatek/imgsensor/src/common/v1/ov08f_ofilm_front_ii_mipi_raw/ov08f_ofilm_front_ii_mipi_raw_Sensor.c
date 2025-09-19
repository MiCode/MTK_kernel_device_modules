/*
 * Copyright (C) 2024 MediaTek Inc.
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
 *     ov08f10shinefrontturmipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * Setting version:
 * ------------
 *   update full pd setting for OV08FEB_03B
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#define PFX "OV08F10_front_camera_sensor"
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

#include "ov08f_ofilm_front_ii_mipi_raw_Sensor.h"

#define _I2C_BUF_SIZE 4096
// kal_uint16 ov08f_i2c_data[_I2C_BUF_SIZE];
// unsigned int _size_to_write;

#define SEAMLESS_ 0
#define SEAMLESS_NO_USE 0

#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __FUNCTION__, ##args)

#define MULTI_WRITE 1

#define FPT_PDAF_SUPPORT 0

#if 1
extern unsigned char fusion_id_front[96];
extern unsigned char sn_front[96];
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV08F_OFILM_FRONT_II_SENSOR_ID,

	.checksum_value = 0x43daf615, /*checksum value for Camera Auto Test*/

	.pre = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 292000000,
	},
	.cap = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 292000000,

	},
	.normal_video = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 292000000,

	},
	.hs_video = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 292000000,

	},
	.slim_video = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 292000000,
    },
    .custom1 = {
        .pclk = 36500000,
        .linelength = 478,
        .framelength = 2544,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1632,
        .grabwindow_height = 1224,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 146000000,
        .max_framerate = 300,
    },

	.margin = 20, /*sensor framelength & shutter margin*/
	.min_shutter = 4, /*min shutter*/
	.min_gain =  64, /*1x gain*/
	.max_gain = 992, /*15.5x gain*/
	.min_gain_iso = 100,
	.gain_step = 1, /*minimum step = 4 in 1x~2x gain*/
	.gain_type = 1, /*to be modify,no gain table for sony*/
	.max_frame_length = 0x3fffff, /* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2, /*isp gain delay frame for AE cycle*/
	.frame_time_delay_frame = 2,
	.ihdr_support = 0, /*1, support; 0,not support*/
	.ihdr_le_firstline = 0, /*1,le first ; 0, se first*/
	//.temperature_support = 0, /* 1, support; 0,not support */
	.sensor_mode_num = 6,
	.cap_delay_frame = 2, /*enter capture delay frame num*/
	.pre_delay_frame = 2, /*enter preview delay frame num*/
	.video_delay_frame = 2, /*enter video delay frame num*/
	.hs_video_delay_frame = 2, /*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 2, /*enter slim video delay frame num*/
    .custom1_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA, /*mclk driving current*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 0, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24, /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_2_LANE, /*mipi lane num*/
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0410, /*current shutter*/
	.gain = 0x40, /*current gain*/
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x20,
	//.vblank_convert = 2504, /* vts to vblank*/
	//.current_ae_effective_frame = 2,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // Preview
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // capture
    {3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 306, 3264, 1836,  0, 0, 3264, 1836},  // video
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // hs_video
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // slim_video
    {3264, 2448,   0,   0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, //custom1 
};

static kal_uint8 read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[1] = {(char)(addr & 0xff)};

	iReadRegI2C(pusendcmd, 1, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	char pusendcmd[2] = {(char)(addr & 0xff), (char)(para & 0xff)};

	iWriteRegI2C(pusendcmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0xfd, 0x01);
	if (imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM1){
		write_cmos_sensor(0x05, (((imgsensor.frame_length - 0x4e3) * 2) & 0xFF00) >> 8);
		write_cmos_sensor(0x06, ((imgsensor.frame_length - 0x4e3 )* 2) & 0xFF);	
	}else{
		write_cmos_sensor(0x05, (((imgsensor.frame_length - 0x9c5) * 2) & 0xFF00) >> 8);
		write_cmos_sensor(0x06, ((imgsensor.frame_length - 0x9c5 )* 2) & 0xFF);
	}
	write_cmos_sensor(0x01, 0x01);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
}

static void set_max_framerate_video(UINT16 framerate, kal_bool min_framelength_en)
{
	set_max_framerate(framerate, min_framelength_en);
	set_dummy();
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable){
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x33, 0x03);
		write_cmos_sensor(0x01, 0x03);
		write_cmos_sensor(0xfd, 0x00);
		write_cmos_sensor(0xe7, 0x03);
		write_cmos_sensor(0xe7, 0x00);
		write_cmos_sensor(0x20, 0x0f);
		write_cmos_sensor(0xa0, 0x01);
	}
	else{
		write_cmos_sensor(0xfd, 0x00);
		write_cmos_sensor(0xa0, 0x00);
		write_cmos_sensor(0x20, 0x0b);
		write_cmos_sensor(0xfd, 0x01);
		mdelay(10);
	}

	return ERROR_NONE;
}

//static int long_exposure_status = 0;

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	pr_debug("shutter1_from_external = %d, frame_length = %d\n", shutter, imgsensor.frame_length);
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	} else {
		imgsensor.frame_length = imgsensor.min_frame_length;
	}
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter) {
		shutter = imgsensor_info.min_shutter;
	}
	shutter =
		(shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305){
			set_max_framerate(296, 0);
		} else if(realtime_fps >= 147 && realtime_fps <= 150){
		set_max_framerate(146, 0);
		}
	}
	if (shutter<=5086){
       write_cmos_sensor(0xfd, 0x02);
       write_cmos_sensor(0x9a, 0x20);
    }else{                                                                                                          
       write_cmos_sensor(0xfd, 0x02);
       write_cmos_sensor(0x9a, 0x30);          
    }
	
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x02, (shutter*2 >> 16) & 0xFF);
	write_cmos_sensor(0x03, (shutter*2 >> 8) & 0xFF);
	write_cmos_sensor(0x04,  shutter*2  & 0xFF);
	write_cmos_sensor(0x01, 0x01);

	LOG_INF("0x05 = 0x%x, 0x06 = 0x%x\n", read_cmos_sensor(0x05), read_cmos_sensor(0x06));
}

static void set_shutter(kal_uint32 shutter)  //should not be kal_uint16 -- can't reach long exp
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	//platform 1xgain = 64, sensor driver 1*gain = 0x100
	iReg = gain*16/BASEGAIN;
	return iReg;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;
	unsigned long flags;

	if (gain < imgsensor_info.min_gain || gain > max_gain) {
		pr_debug("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x24, (reg_gain & 0xFF));
	write_cmos_sensor(0x01, 0x01);
	return gain;
}

/* ITD: Modify Dualcam By Jesse 190924 Start */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 target_frame_length)
{

	spin_lock(&imgsensor_drv_lock);
	if (target_frame_length > 1)
		imgsensor.dummy_line = target_frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_shutter(shutter);
}
/* ITD: Modify Dualcam By Jesse 190924 End */

static void ihdr_write_shutter_gain(kal_uint16 le,
	kal_uint16 se, kal_uint16 gain)
{
}

static void night_mode(kal_bool enable)
{
}

static void init_setting(void)
{
	pr_debug("%s start\n", __func__);

	pr_debug("%s end\n", __func__);
}

static void preview_setting(void)
{
	pr_debug("%s start\n", __func__);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);	
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c); 
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x06, 0x56);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x0d);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x0f);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x0b);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);       
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	pr_debug("%s end\n", __func__);
}

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("%s start currefps = %d\n", __func__,currefps);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);	
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c); 
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x06, 0x56);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x0d);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x0f);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x0b);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);       
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	pr_debug("%s end\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("%s start currefps = %d\n", __func__,currefps);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c); 
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x06, 0x56);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x0d);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x0f);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x0b);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x01);
	write_cmos_sensor(0xa1, 0x3a);
	write_cmos_sensor(0xa2, 0x07);
	write_cmos_sensor(0xa3, 0x2c);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x07);
	write_cmos_sensor(0x91, 0x2c);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x01);
	pr_debug("%s end\n", __func__);
}

static void hs_video_setting(void)
{
	pr_debug("%s start\n", __func__);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);	
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c); 
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x06, 0x56);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x0d);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x0f);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x0b);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);       
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	pr_debug("%s end\n", __func__);
}

static void slim_video_setting(void)
{
	pr_debug("%s start\n", __func__);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);	
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c); 
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x08);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x06, 0x56);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x0d);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x0f);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x0b);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x06);
	write_cmos_sensor(0x09, 0x11);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa1, 0x08);
	write_cmos_sensor(0xa2, 0x09);
	write_cmos_sensor(0xa3, 0x90);
	write_cmos_sensor(0xa5, 0x08);
	write_cmos_sensor(0xa6, 0x0c);
	write_cmos_sensor(0xa7, 0xc0);       
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x0c);
	write_cmos_sensor(0x8f, 0xc0);
	write_cmos_sensor(0x90, 0x09);
	write_cmos_sensor(0x91, 0x90);
	write_cmos_sensor(0xb7, 0x02);
	pr_debug("%s end\n", __func__);
}

static void custom1_setting(void)
{
	pr_debug("%s start\n", __func__);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	msleep(5);	
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x10, 0x08);
	write_cmos_sensor(0x11, 0x5e);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x1d, 0x00);
	write_cmos_sensor(0x1c, 0x19);
	write_cmos_sensor(0xb2, 0x0f);
	write_cmos_sensor(0x13, 0x15);
	write_cmos_sensor(0x14, 0x2c);
	write_cmos_sensor(0x1a, 0x16);
	write_cmos_sensor(0x1b, 0x6d);
	write_cmos_sensor(0x1e, 0x13);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x1a, 0x0a);
	write_cmos_sensor(0x1b, 0x08);
	write_cmos_sensor(0x2a, 0x01);
	write_cmos_sensor(0x2b, 0x9a);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x05);
	write_cmos_sensor(0x04, 0x08);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x06, 0x1a);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x24, 0xff);	
	write_cmos_sensor(0x31, 0x06);
	write_cmos_sensor(0x32, 0x03);//mirror flip
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0x42, 0x5d);
	write_cmos_sensor(0x44, 0x81);
	write_cmos_sensor(0x46, 0x3f);
	write_cmos_sensor(0x47, 0x0f);
	write_cmos_sensor(0x48, 0x0c);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x1e);
	write_cmos_sensor(0xb3, 0x1b);
	write_cmos_sensor(0xc3, 0x2c);
	write_cmos_sensor(0xd2, 0xfc);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x50, 0x1c);
	write_cmos_sensor(0x53, 0x1c);
	write_cmos_sensor(0x54, 0x04);
	write_cmos_sensor(0x55, 0x04);
	write_cmos_sensor(0x57, 0x87);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x67, 0x05);
	write_cmos_sensor(0x7a, 0x02);
	write_cmos_sensor(0x7c, 0x0a);
	write_cmos_sensor(0x90, 0x3a);
	write_cmos_sensor(0x91, 0x12);
	write_cmos_sensor(0x92, 0x16);
	write_cmos_sensor(0x94, 0x12);
	write_cmos_sensor(0x95, 0x40);
	write_cmos_sensor(0x97, 0x03);
	write_cmos_sensor(0x98, 0x48);
	write_cmos_sensor(0x9a, 0x02);
	write_cmos_sensor(0x9c, 0x0c);
	write_cmos_sensor(0x9e, 0x2d);
	write_cmos_sensor(0xca, 0x12);
	write_cmos_sensor(0xcb, 0x12);
	write_cmos_sensor(0xcc, 0x12);
	write_cmos_sensor(0xcd, 0x16);
	write_cmos_sensor(0xce, 0x12);
	write_cmos_sensor(0xcf, 0x10);
	write_cmos_sensor(0xd0, 0x10);
	write_cmos_sensor(0xd1, 0x14);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x03);
	write_cmos_sensor(0x09, 0x08);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0x00);
	write_cmos_sensor(0x50, 0x02);
	write_cmos_sensor(0x51, 0x03);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x78);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x78);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x78);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x78);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x00, 0x27);
	write_cmos_sensor(0x01, 0x34);
	write_cmos_sensor(0x02, 0x88);
	write_cmos_sensor(0x1a, 0x21);
	write_cmos_sensor(0x1b, 0x4b);
	write_cmos_sensor(0x1c, 0xd5);
	write_cmos_sensor(0x1d, 0xdb);
	write_cmos_sensor(0x1f, 0x31);
	write_cmos_sensor(0x20, 0xcc);
	write_cmos_sensor(0xfd, 0x0f);
	write_cmos_sensor(0x03, 0x07);
	write_cmos_sensor(0x05, 0x0a);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x13, 0x00);
	write_cmos_sensor(0x14, 0x13);
	write_cmos_sensor(0x08, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x17, 0x0c);
	write_cmos_sensor(0x18, 0x0c);
	write_cmos_sensor(0x19, 0x0c);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xfd, 0x04);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x0a, 0x10);
	write_cmos_sensor(0x0b, 0x11);
	write_cmos_sensor(0x0c, 0x12);
	write_cmos_sensor(0x0d, 0xfd);
	write_cmos_sensor(0xfd, 0x03);
	write_cmos_sensor(0x9d, 0x0f);
	write_cmos_sensor(0x9f, 0x20);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x00, 0x00);
	write_cmos_sensor(0x01, 0x1c);
	write_cmos_sensor(0x02, 0x01);
	write_cmos_sensor(0x03, 0xff);
	write_cmos_sensor(0x1a, 0x20);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xa2, 0x04);
	write_cmos_sensor(0xa3, 0xc8);
	write_cmos_sensor(0xa5, 0x04);
	write_cmos_sensor(0xa6, 0x06);
	write_cmos_sensor(0xa7, 0x60);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x17);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x38);
	write_cmos_sensor(0x8e, 0x06);
	write_cmos_sensor(0x8f, 0x60);
	write_cmos_sensor(0x90, 0x04);
	write_cmos_sensor(0x91, 0xc8);
	write_cmos_sensor(0x93, 0x0e);
	write_cmos_sensor(0x94, 0x77);
	write_cmos_sensor(0x95, 0x77);
	write_cmos_sensor(0x96, 0x10);
	write_cmos_sensor(0x98, 0x88);
	write_cmos_sensor(0x9c, 0x1a);
	write_cmos_sensor(0xb7, 0x02);
	pr_debug("%s end\n", __func__);
}

static void sensor_init(void)
{
	LOG_INF("%s E\n", __func__);
	init_setting();
	LOG_INF("%s X\n", __func__);
}

static kal_uint32 return_sensor_id(void)
{
	write_cmos_sensor(0xfd, 0x00);
		return (((read_cmos_sensor(0x00) << 8) | (read_cmos_sensor(0x01))));
}

#if 1
static kal_uint16 read_cmos_sensor_ov08f_ofilm_otp(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, 0xA0);
	return get_byte;
}

static void ov08f_ofilm_fusion_id_read(void)
{
	int i;
	for (i=0; i<16; i++) {
		fusion_id_front[i] = read_cmos_sensor_ov08f_ofilm_otp(0x10+i);
	pr_err("%s %d zengx fusion_id_front[%d]=0x%2x\n",__func__, __LINE__, i, fusion_id_front[i]);
	}
}
static void ov08f_ofilm_sn_id_read(void)
{
	int i;
	for (i=0; i<25; i++) {
	        sn_front[i] = read_cmos_sensor_ov08f_ofilm_otp(0xEEC+i);
		pr_err("%s %d zengx sn_front[%d]=0x%2x\n",__func__, __LINE__, i, sn_front[i]);
	}
}
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	LOG_INF("[get_imgsensor_id] ");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id  : 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
#if 1
                    ov08f_ofilm_fusion_id_read();
                    ov08f_ofilm_sn_id_read();
#endif
				return ERROR_NONE;
			}
			LOG_ERR("Read sensor id fail, i2c write id: 0x%x,sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
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

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	pr_debug("%s +\n", __func__);

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
			LOG_ERR("open:Read sensor id fail open i2c write id: 0x%x, id: 0x%x\n",
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

	// write_sensor_PDC();
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("close E");

	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
} /* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
}

static kal_uint32 hs_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
}

static kal_uint32 custom1(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	LOG_INF("%s X\n", __func__);
	return ERROR_NONE;
}
static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
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
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);
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
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;


	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
#if FPT_PDAF_SUPPORT
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	sensor_info->PDAF_Support = 2;
#else
	sensor_info->PDAF_Support = 0;
#endif

	//sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;   // 0 is default 1x
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
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
	return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;

	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);

	set_max_framerate_video(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
	UINT16 framerate)
{
	pr_debug("enable = %d, framerate = %d\n",
		enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frameHeight;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.normal_video.framelength) ?
		(frameHeight - imgsensor_info.normal_video.framelength):0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.cap.framelength) ?
			(frameHeight - imgsensor_info.cap.framelength):0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength) ?
			(frameHeight - imgsensor_info.hs_video.framelength):0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.slim_video.framelength) ?
			(frameHeight - imgsensor_info.slim_video.framelength):0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frameHeight = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight > imgsensor_info.custom1.framelength) ?
			(frameHeight - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength):0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("[3058]scenario_id = %d\n", scenario_id);

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
	LOG_INF("set_test_pattern_mode enable: %d\n", enable);

	if(enable){
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x12, 0x01);
	}
	else{
		write_cmos_sensor(0xfd, 0x01);
		write_cmos_sensor(0x12, 0x00);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 ov08f_ana_gain_table_16x[] = {
	1024,  1088,  1152,  1216,
	1280,  1344,  1408,  1472,
	1536,  1600,  1664,  1728,
	1792,  1856,  1920,  1984,
	2048,  2176,  2304,  2432,
	2560,  2688,  2816,  2944,
	3072,  3200,  3328,  3456,
	3584,  3712,  3840,  3968,
	4096,  4352,  4608,  4864,
	5120,  5376,  5632,  5888,
	6144,  6400,  6656,  6912,
	7168,  7424,  7680,  7936,
	8192,  8704,  9216,  9728,
	10240, 10752, 11264, 11776,
	12288, 12800, 13312, 13824,
	14336, 14848, 15360, 15872
};

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	//INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	// UINT32 *pAeCtrls = NULL;
	UINT32 *pScenarios = NULL;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

#if FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif
	//pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		pr_debug("use_my_gain_table,feature_id = %d\n", feature_id);
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(ov08f_ana_gain_table_16x);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)ov08f_ana_gain_table_16x,
			sizeof(ov08f_ana_gain_table_16x));
		}
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		*(feature_data + 1) = 1;
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*pScenarios = 0xff;
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %d\n",
			*feature_data, *pScenarios);
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
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(feature_data + 2) = 2;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
		default:
			*(feature_data + 2) = 1;
			break;
		}
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
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
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
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
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
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		pr_debug("current fps :%d\n", imgsensor.current_fps);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:

		wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
	break;
	//case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
	//	*feature_return_para_32 = imgsensor.current_ae_effective_frame;
	//	break;
	//case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
	//	memcpy(feature_return_para_32, &imgsensor.ae_frm_mode,
	//		sizeof(struct IMGSENSOR_AE_FRM_MODE));
	//	break;
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

	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length((UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;

	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			break;
		default:
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

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

UINT32 OV08F_OFILM_FRONT_II_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;

	return ERROR_NONE;
}