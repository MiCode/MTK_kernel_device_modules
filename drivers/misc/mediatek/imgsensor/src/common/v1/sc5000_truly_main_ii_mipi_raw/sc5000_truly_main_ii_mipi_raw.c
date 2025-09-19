/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 SC5000CSmipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6873
*
* Description:
* ------------
*---------------------------------------------------------------------------
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
#include <linux/types.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "sc5000_truly_main_ii_mipi_raw.h"



#define FPTPDAFSUPPORT

#define MULTI_WRITE 0
#define OTP_DATA_NUMBER 9

#define PFX "SC5000CS_TRULY_camera_sensor"

#define LOG_INF(format, args...)pr_err(PFX "[%s] " format, __func__, ##args)

#if 1
extern unsigned char fusion_id_main[96];
extern unsigned char sn_main[96];
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id =  SC5000_TRULY_MAIN_II_SENSOR_ID,
	.checksum_value = 0xdb9c643,
	.pre = {
		.pclk = 120000000,
		.linelength = 1240,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 120000000,
		.linelength = 1240,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 300,	
	},
	.cap1 = {
		.pclk = 120000000,
		.linelength = 1240,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 300,	
	},
	.normal_video = {
		.pclk = 120000000,
		.linelength = 1240,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 120000000,
		.linelength = 620,
		.framelength = 1610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 120000000,
		.linelength = 620,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 600,
	},
	.custom1 = {
		.pclk = 120000000,
		.linelength = 1240,
		.framelength = 4030,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 604800000,
		.max_framerate = 240,
	},
	.margin = 8,
	.min_shutter = 3,
	.min_gain = 64, /*1x gain*/
	.max_gain = 1728, /*27x gain*/
	.min_gain_iso = 50,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 2,
	.max_frame_length = 0xfffff7,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.frame_time_delay_frame = 1,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 6,
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_2MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20,0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0200,
	.gain = 0x0100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE,
	.i2c_write_id = 0x20,
//cxc long exposure >
	.current_ae_effective_frame = 2,
//cxc long exposure <
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[6] = {
/* preview mode setting(4096*3072) PD size: 1008*756*/
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0c00, 0x01, 0x00, 0x0000, 0x0000,
		0x02, 0x30, 0x04EC, 0x02F4, 0x03, 0x00, 0x0000, 0x0000
	},
/* capture mode setting(4080*3072) PD size: 1008*756*/
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0c00, 0x01, 0x00, 0x0000, 0x0000,
		0x02, 0x30, 0x04EC, 0x02F4, 0x03, 0x00, 0x0000, 0x0000
	},
/* normal_video mode setting(4096*2304) PD size: 1008*576**/
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0900, 0x01, 0x00, 0x0000, 0x0000,
		0x02, 0x30, 0x04EC, 0x0240, 0x03, 0x00, 0x0000, 0x0000
	},
/* high_speed_video mode setting */ //no support pd
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x0500, 0x02D0, 0x01, 0x00, 0x0000, 0x0000,
		0x00, 0x30, 0x026C, 0x02E0, 0x03, 0x00, 0x0000, 0x0000
	},
/* slim_video mode setting */ //no support pd
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x0780, 0x0438, 0x01, 0x00, 0x0000, 0x0000,
		0x00, 0x30, 0x026C, 0x02E0, 0x03, 0x00, 0x0000, 0x0000
	},
/* custom1 mode setting */
	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x1000, 0x0c00, 0x01, 0x00, 0x0000, 0x0000,
		0x02, 0x30, 0x03F0, 0x02F4, 0x03, 0x00, 0x0000, 0x0000
	},
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
//preview
{ 4096, 3072,0,0, 4096, 3072, 4096, 3072,0, 0, 4096, 3072, 0, 0, 4096, 3072},
//capture
{ 4096, 3072,0,0, 4096, 3072, 4096, 3072,0, 0, 4096, 3072, 0, 0, 4096, 3072},
//normal_video
{ 4096, 3072,0,0, 4096, 3072, 4096, 3072,0, 384, 4096, 2304, 0, 0, 4096, 2304},
//hs_video
{4096, 3072,0,0,  4096, 3072, 2048, 1536,64, 228,1920, 1080, 0, 0, 1920, 1080},
//slim_video
{4096, 3072,0,0,  4096, 3072, 2048, 1536,64, 228,1920, 1080, 0, 0, 1920, 1080},
//custom1
{ 4096, 3072,0,0, 4096, 3072, 4096, 3072,0, 0, 4096, 3072, 0, 0, 4096, 3072},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 32,
	.i4OffsetY = 24,
	.i4PitchX = 8,
	.i4PitchY = 8,
	.i4PairNum = 2,
	.i4SubBlkW = 8,
	.i4SubBlkH = 4,
	.i4PosL = {
		{38, 26},
		{34, 30},
	},
	.i4PosR = {
		{39, 26},
		{35, 30},
	},
	.iMirrorFlip = IMAGE_HV_MIRROR,
	//.iMirrorFlip = IMAGE_NORMAL,
	.i4BlockNumX = 504,
	.i4BlockNumY = 378,
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_16_9 = {
	.i4OffsetX = 32,
	.i4OffsetY = 0,
	.i4PitchX = 8,
	.i4PitchY = 8,
	.i4PairNum = 2,
	.i4SubBlkW = 8,
	.i4SubBlkH = 4,
	.i4PosL = {
		{38, 2},
		{34, 6},
	},
	.i4PosR = {
		{39, 2},
		{35, 6},
	},
	.iMirrorFlip = IMAGE_HV_MIRROR,
	//.iMirrorFlip = IMAGE_NORMAL,
	.i4BlockNumX = 504,
	.i4BlockNumY = 288,
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020/*trans max is 255, each 3 bytes*/
#else
#define I2C_BUFFER_LEN 4
#endif

/*
static kal_uint16 sc5000cs_multi_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
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
		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				4, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 4, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}
*/

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *) &get_byte, 1,
				imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *) &get_byte, 1,
				imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static void write_cmos_sensor_byte(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
	(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8),
		(char)(para & 0xFF)
	};

	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

#if 1
static kal_uint16 read_cmos_sensor_sc5000cs(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *) &get_byte, 1,
				0xA0, imgsensor_info.i2c_speed);
	return get_byte;
}
static void sc5000cs_fusion_id_read(void)
{
	int i;
	for (i=0; i<11; i++) {
		fusion_id_main[i] = read_cmos_sensor_sc5000cs(0x0010+i);
		pr_info("lss %s addr = 0x%4x fusion_id_main[%d]=0x%2x\n",__func__, 0x10 + i, i, fusion_id_main[i]);
	}
}
static void sc5000cs_sn_read(void)
{
	int i;
	for (i=0; i<25; i++) {
		sn_main[i] = read_cmos_sensor_sc5000cs(0x3FD8+i);
		pr_info("lss %s addr = 0x%4x sn_main[%d]=0x%2x\n",__func__, 0x3FD8 + i, i, sn_main[i]);
	}
}
#endif
static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line,
			imgsensor.dummy_pixel);

	write_cmos_sensor_byte(0x326d, (imgsensor.frame_length >> 16) & 0xff);
	write_cmos_sensor_byte(0x320e, (imgsensor.frame_length >> 8) & 0xff);
	write_cmos_sensor_byte(0x320f, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor_byte(0x320c, imgsensor.line_length >> 8);
	write_cmos_sensor_byte(0x320d, imgsensor.line_length & 0xFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable(%d)\n",
			framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length >
		 imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
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


static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_byte(0x0100, 0X01);
	else
		write_cmos_sensor_byte(0x0100, 0x00);
	 mdelay(10);//delay 10ms
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

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
		write_cmos_sensor_byte(0x326d, (imgsensor.frame_length >> 16) & 0xff);
		write_cmos_sensor_byte(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor_byte(0x320f, imgsensor.frame_length & 0xFF);
		}

	} else{
		// Extend frame length

		// ADD ODIN
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
		// ADD END
		write_cmos_sensor_byte(0x326d, (imgsensor.frame_length >> 16) & 0xff);
		write_cmos_sensor_byte(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor_byte(0x320f, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	//shutter = shutter *2;
	write_cmos_sensor_byte(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor_byte(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor_byte(0x3e01, (shutter >> 4)&0xFF);
	write_cmos_sensor_byte(0x3e02, (shutter<<4) & 0xF0);

	LOG_INF("shutter =%d, framelength =%d",
		shutter, imgsensor.frame_length);

}



/*************************************************************************
*FUNCTION
*set_shutter
*
*DESCRIPTION
*This function set e-shutter of sensor to change exposure time.
*
*PARAMETERS
*iShutter : exposured lines
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);

}

/*************************************************************************
*FUNCTION
*set_shutter_frame_length
*
*DESCRIPTION
*for frame &3A sync
*
*************************************************************************/ 
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length,kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	kal_bool autoflicker_closed = KAL_FALSE;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter =
		(shutter < imgsensor_info.min_shutter)
		 ? imgsensor_info.min_shutter : shutter;
	shutter =
		(shutter >
		 (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
			 imgsensor_info.margin) : shutter;

	if (autoflicker_closed) {
		realtime_fps =
			imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
			write_cmos_sensor_byte(0x326d, (imgsensor.frame_length >> 16) & 0xff);
		    write_cmos_sensor_byte(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		    write_cmos_sensor_byte(0x320f, imgsensor.frame_length & 0xFF);
		}
	} else{
		write_cmos_sensor_byte(0x326d, (imgsensor.frame_length >> 16) & 0xff);
		write_cmos_sensor_byte(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor_byte(0x320f, imgsensor.frame_length & 0xFF);
    }

	write_cmos_sensor_byte(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor_byte(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor_byte(0x3e01, (shutter >> 4)&0xFF);
	write_cmos_sensor_byte(0x3e02, (shutter<<4) & 0xF0);
}

#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain / 2;

	return (kal_uint16) reg_gain;
}
#endif
/*************************************************************************
*FUNCTION
*set_gain
*
*DESCRIPTION
*This function is to set global gain to sensor.
*
*PARAMETERS
*iGain : sensor global gain(base: 0x40)
*
*RETURNS
*the actually gain set to sensor.
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{

	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 27 * BASEGAIN) {

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 27 * BASEGAIN)
			gain = 27 * BASEGAIN;
	}
    reg_gain = gain << 1;
	write_cmos_sensor_byte(0x3e08,(reg_gain>>8));	
	write_cmos_sensor_byte(0x3e09,(reg_gain&0xFF));	

    if(read_cmos_sensor(0x0100) == 0){
	   LOG_INF("read off 0x0100 = 0x%x\n", read_cmos_sensor(0x0100));
		write_cmos_sensor_byte(0x3367, 0x3e);
	}
	else{
		LOG_INF("read on 0x0100 = 0x%x\n", read_cmos_sensor(0x0100));
		write_cmos_sensor_byte(0x3367, 0x04);
	}
    LOG_INF("read 0x3367 = 0x%x\n", read_cmos_sensor(0x3367));
    if(read_cmos_sensor(0x3220)==0x02){
	if(reg_gain<2048)
	{
		write_cmos_sensor_byte(0x3800, 0x00);
		write_cmos_sensor_byte(0x372b, 0xd3);
		write_cmos_sensor_byte(0x3800, 0x10);
		write_cmos_sensor_byte(0x3800, 0x40);
	}
	else{
		write_cmos_sensor_byte(0x3800, 0x00);
		write_cmos_sensor_byte(0x372b, 0xdf);
		write_cmos_sensor_byte(0x3800, 0x10);
		write_cmos_sensor_byte(0x3800, 0x40);
	}
    }
	LOG_INF("read 0x372b = 0x%x\n", read_cmos_sensor(0x372b));

    spin_lock(&imgsensor_drv_lock); 
    imgsensor.gain = reg_gain; 
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d ,reg_gain = 0x%x ,again1 = 0x%x , again2 = 0x%x\n ", gain, reg_gain,read_cmos_sensor(0x3e08),read_cmos_sensor(0x3e09));

	return gain;
}


static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
        write_cmos_sensor_byte(0x3221, 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_byte(0x3221, 0x06);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_byte(0x3221, 0x60);
		break;
	case IMAGE_HV_MIRROR:
        write_cmos_sensor_byte(0x3221, 0x66);
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
		break;
	}
}

/*************************************************************************
*FUNCTION
*night_mode
*
*DESCRIPTION
*This function night mode of sensor.
*
*PARAMETERS
*bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{

}
#endif


static void sensor_init(void)
{
	LOG_INF("sensor_init start\n");
	write_cmos_sensor_byte(0x0100,0x00);
	LOG_INF("sensor_init end\n");
}



static void preview_setting(void)
{
	LOG_INF("preview_setting start\n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x10);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x10);
	write_cmos_sensor_byte(0x3059,0x86);
	write_cmos_sensor_byte(0x305a,0x43);
	write_cmos_sensor_byte(0x305b,0x52);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3208,0x10);
	write_cmos_sensor_byte(0x3209,0x00);
	write_cmos_sensor_byte(0x320a,0x0c);
	write_cmos_sensor_byte(0x320b,0x00);
	write_cmos_sensor_byte(0x320c,0x04);
	write_cmos_sensor_byte(0x320d,0xd8);
	write_cmos_sensor_byte(0x320e,0x0c);
	write_cmos_sensor_byte(0x320f,0x98);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x30);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x22);
	write_cmos_sensor_byte(0x321a,0x10);
	write_cmos_sensor_byte(0x3220,0x02);
	write_cmos_sensor_byte(0x3225,0x21);
	write_cmos_sensor_byte(0x3226,0x05);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x20);
	write_cmos_sensor_byte(0x325f,0x40);
	write_cmos_sensor_byte(0x3280,0x0d);
	write_cmos_sensor_byte(0x3284,0x02);
	write_cmos_sensor_byte(0x3287,0x20);
	write_cmos_sensor_byte(0x329d,0x8c);
	write_cmos_sensor_byte(0x32d1,0x30);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x3309,0x60);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x331f,0x51);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x3399,0x0a);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x18);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x08);
	write_cmos_sensor_byte(0x34bb,0x01);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0xbc);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3636,0xc5);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x41);
	write_cmos_sensor_byte(0x3672,0x21);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x34);
	write_cmos_sensor_byte(0x3691,0x46);
	write_cmos_sensor_byte(0x3692,0x46);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0xb8);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x0b);
	write_cmos_sensor_byte(0x36c0,0x18);
	write_cmos_sensor_byte(0x36c1,0x18);
	write_cmos_sensor_byte(0x36c2,0x54);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xd3);
	write_cmos_sensor_byte(0x3771,0x1f);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x02);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0xc1);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x90);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3b09,0x00);
	write_cmos_sensor_byte(0x3b0a,0x00);
	write_cmos_sensor_byte(0x3b0b,0x00);
	write_cmos_sensor_byte(0x3b0c,0x00);
	write_cmos_sensor_byte(0x3b0d,0x10);
	write_cmos_sensor_byte(0x3b0e,0x10);
	write_cmos_sensor_byte(0x3b0f,0x10);
	write_cmos_sensor_byte(0x3b10,0x10);
	write_cmos_sensor_byte(0x3b11,0x20);
	write_cmos_sensor_byte(0x3b12,0x20);
	write_cmos_sensor_byte(0x3b13,0x20);
	write_cmos_sensor_byte(0x3b14,0x20);
	write_cmos_sensor_byte(0x3b15,0x30);
	write_cmos_sensor_byte(0x3b16,0x30);
	write_cmos_sensor_byte(0x3b17,0x30);
	write_cmos_sensor_byte(0x3b18,0x30);
	write_cmos_sensor_byte(0x3b49,0x03);
	write_cmos_sensor_byte(0x3b4c,0x03);
	write_cmos_sensor_byte(0x3b4d,0xff);
	write_cmos_sensor_byte(0x3b50,0x03);
	write_cmos_sensor_byte(0x3b51,0x7a);
	write_cmos_sensor_byte(0x3b52,0x03);
	write_cmos_sensor_byte(0x3b53,0x7b);
	write_cmos_sensor_byte(0x3b54,0x03);
	write_cmos_sensor_byte(0x3b55,0x80);
	write_cmos_sensor_byte(0x3b5a,0x03);
	write_cmos_sensor_byte(0x3b5b,0xf8);
	write_cmos_sensor_byte(0x3b5c,0x03);
	write_cmos_sensor_byte(0x3b5d,0xf9);
	write_cmos_sensor_byte(0x3b64,0x03);
	write_cmos_sensor_byte(0x3b65,0x7c);
	write_cmos_sensor_byte(0x3b66,0x03);
	write_cmos_sensor_byte(0x3b67,0x7b);
	write_cmos_sensor_byte(0x3b68,0x03);
	write_cmos_sensor_byte(0x3b69,0x80);
	write_cmos_sensor_byte(0x3b6e,0x03);
	write_cmos_sensor_byte(0x3b6f,0xf8);
	write_cmos_sensor_byte(0x3b70,0x03);
	write_cmos_sensor_byte(0x3b71,0xfc);
	write_cmos_sensor_byte(0x3c00,0x08);
	write_cmos_sensor_byte(0x3c06,0x07);
	write_cmos_sensor_byte(0x3c07,0x02);
	write_cmos_sensor_byte(0x3c2c,0x47);
	write_cmos_sensor_byte(0x3c65,0x05);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0xb1);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x3e25,0xff);
	write_cmos_sensor_byte(0x3e26,0xff);
	write_cmos_sensor_byte(0x3e29,0x08);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x38);
	write_cmos_sensor_byte(0x420f,0x78);
	write_cmos_sensor_byte(0x4210,0x12);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x08);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x10);
	write_cmos_sensor_byte(0x421d,0x10);
	write_cmos_sensor_byte(0x421e,0x12);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x43ac,0x80);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4503,0x2c);
	write_cmos_sensor_byte(0x4506,0xa2);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4512,0x01);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0x0f);
	write_cmos_sensor_byte(0x5002,0xd8);
	write_cmos_sensor_byte(0x5003,0xf0);
	write_cmos_sensor_byte(0x5004,0x00);
	write_cmos_sensor_byte(0x5009,0x0f);
	write_cmos_sensor_byte(0x5016,0x5a);
	write_cmos_sensor_byte(0x5017,0x5a);
	write_cmos_sensor_byte(0x5018,0x00);
	write_cmos_sensor_byte(0x5019,0x21);
	write_cmos_sensor_byte(0x501a,0x0f);
	write_cmos_sensor_byte(0x501b,0xf0);
	write_cmos_sensor_byte(0x501c,0x00);
	write_cmos_sensor_byte(0x501d,0x21);
	write_cmos_sensor_byte(0x501e,0x0b);
	write_cmos_sensor_byte(0x501f,0xf0);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5075,0xcd);
	write_cmos_sensor_byte(0x507f,0xe2);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x0c);
	write_cmos_sensor_byte(0x53c5,0x40);
	write_cmos_sensor_byte(0x5401,0x08);
	write_cmos_sensor_byte(0x540a,0x0c);
	write_cmos_sensor_byte(0x540b,0x5f);
	write_cmos_sensor_byte(0x540e,0x03);
	write_cmos_sensor_byte(0x540f,0x70);
	write_cmos_sensor_byte(0x5414,0x0c);
	write_cmos_sensor_byte(0x5415,0x80);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x541b,0x00);
	write_cmos_sensor_byte(0x541c,0x49);
	write_cmos_sensor_byte(0x541d,0x0c);
	write_cmos_sensor_byte(0x541e,0x18);
	write_cmos_sensor_byte(0x5501,0x08);
	write_cmos_sensor_byte(0x550a,0x0c);
	write_cmos_sensor_byte(0x550b,0x5f);
	write_cmos_sensor_byte(0x550e,0x02);
	write_cmos_sensor_byte(0x550f,0x00);
	write_cmos_sensor_byte(0x5514,0x0c);
	write_cmos_sensor_byte(0x5515,0x80);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x551b,0x00);
	write_cmos_sensor_byte(0x551c,0x49);
	write_cmos_sensor_byte(0x551d,0x0c);
	write_cmos_sensor_byte(0x551e,0x18);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x00);
	write_cmos_sensor_byte(0x5809,0x49);
	write_cmos_sensor_byte(0x580a,0x0c);
	write_cmos_sensor_byte(0x580b,0x18);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x00);
	write_cmos_sensor_byte(0x5827,0x00);
	write_cmos_sensor_byte(0x5828,0x0c);
	write_cmos_sensor_byte(0x5829,0x5f);
	write_cmos_sensor_byte(0x5f00,0x05);
	write_cmos_sensor_byte(0x5f06,0x00);
	write_cmos_sensor_byte(0x5f07,0x42);
	write_cmos_sensor_byte(0x5f08,0x1f);
	write_cmos_sensor_byte(0x5f09,0xe1);
	write_cmos_sensor_byte(0x5f0c,0x00);
	write_cmos_sensor_byte(0x5f0d,0x92);
	write_cmos_sensor_byte(0x5f0e,0x18);
	write_cmos_sensor_byte(0x5f0f,0x31);
	write_cmos_sensor_byte(0x5f16,0x00);
	write_cmos_sensor_byte(0x5f17,0x00);
	write_cmos_sensor_byte(0x5f18,0x20);
	write_cmos_sensor_byte(0x5f19,0x1f);
	write_cmos_sensor_byte(0x5f1a,0x00);
	write_cmos_sensor_byte(0x5f1b,0x00);
	write_cmos_sensor_byte(0x5f1c,0x18);
	write_cmos_sensor_byte(0x5f1d,0xbf);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x61b9,0xe0);
	write_cmos_sensor_byte(0x61bd,0x02);
	write_cmos_sensor_byte(0x61bf,0x00);
	write_cmos_sensor_byte(0x61e1,0x03);
	write_cmos_sensor_byte(0x61e2,0xf0);
	write_cmos_sensor_byte(0x61e3,0x02);
	write_cmos_sensor_byte(0x61e4,0xf4);
	write_cmos_sensor_byte(0x61e5,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
	write_cmos_sensor_byte(0x6e00,0x03);
	write_cmos_sensor_byte(0x6e05,0x66);
	write_cmos_sensor_byte(0x6e06,0x66);
	write_cmos_sensor_byte(0x6e07,0x66);
	write_cmos_sensor_byte(0x6e08,0x66);
	write_cmos_sensor_byte(0x6e09,0xff);
	write_cmos_sensor_byte(0x6e0a,0x19);
	write_cmos_sensor_byte(0x6e0b,0x19);
	write_cmos_sensor_byte(0x6e0c,0x19);
	write_cmos_sensor_byte(0x6e0d,0x19);
	write_cmos_sensor_byte(0x6e0e,0x19);
	write_cmos_sensor_byte(0x6e0f,0x19);
	write_cmos_sensor_byte(0x6e10,0x19);
	write_cmos_sensor_byte(0x6e11,0x19);
	write_cmos_sensor_byte(0x6e12,0xff);
	write_cmos_sensor_byte(0x6e13,0x19);
	write_cmos_sensor_byte(0x6e14,0x19);
	write_cmos_sensor_byte(0x6e15,0x19);
	write_cmos_sensor_byte(0x6e16,0x19);
	write_cmos_sensor_byte(0x6e17,0x19);
	write_cmos_sensor_byte(0x6e18,0x19);
	write_cmos_sensor_byte(0x6e19,0x19);
	write_cmos_sensor_byte(0x6e1a,0x19);
	write_cmos_sensor_byte(0x6e1b,0xff);
	write_cmos_sensor_byte(0x6e1c,0x19);
	write_cmos_sensor_byte(0x6e1d,0x19);
	write_cmos_sensor_byte(0x6e1e,0x19);
	write_cmos_sensor_byte(0x6e1f,0x19);
	write_cmos_sensor_byte(0x6e20,0x19);
	write_cmos_sensor_byte(0x6e21,0x19);
	write_cmos_sensor_byte(0x6e22,0x19);
	write_cmos_sensor_byte(0x6e23,0x19);
	write_cmos_sensor_byte(0x6e24,0xff);
	write_cmos_sensor_byte(0x6e25,0x19);
	write_cmos_sensor_byte(0x6e26,0x19);
	write_cmos_sensor_byte(0x6e27,0x19);
	write_cmos_sensor_byte(0x6e28,0x19);
	write_cmos_sensor_byte(0x6e29,0x19);
	write_cmos_sensor_byte(0x6e2a,0x19);
	write_cmos_sensor_byte(0x6e2b,0x19);
	write_cmos_sensor_byte(0x6e2c,0x19);
	write_cmos_sensor_byte(0x6e2d,0xff);
	write_cmos_sensor_byte(0x6e2e,0x19);
	write_cmos_sensor_byte(0x6e2f,0x19);
	write_cmos_sensor_byte(0x6e30,0x19);
	write_cmos_sensor_byte(0x6e31,0x19);
	write_cmos_sensor_byte(0x6e32,0x19);
	write_cmos_sensor_byte(0x6e33,0x19);
	write_cmos_sensor_byte(0x6e34,0x19);
	write_cmos_sensor_byte(0x6e35,0x19);
	write_cmos_sensor_byte(0x6e36,0xff);
	write_cmos_sensor_byte(0x6e37,0x19);
	write_cmos_sensor_byte(0x6e38,0x19);
	write_cmos_sensor_byte(0x6e39,0x19);
	write_cmos_sensor_byte(0x6e3a,0x19);
	write_cmos_sensor_byte(0x6e3b,0x19);
	write_cmos_sensor_byte(0x6e3c,0x19);
	write_cmos_sensor_byte(0x6e3d,0x19);
	write_cmos_sensor_byte(0x6e3e,0x19);
	write_cmos_sensor_byte(0x6e3f,0xff);
	write_cmos_sensor_byte(0x6e40,0x19);
	write_cmos_sensor_byte(0x6e41,0x19);
	write_cmos_sensor_byte(0x6e42,0x19);
	write_cmos_sensor_byte(0x6e43,0x19);
	write_cmos_sensor_byte(0x6e44,0x19);
	write_cmos_sensor_byte(0x6e45,0x19);
	write_cmos_sensor_byte(0x6e46,0x19);
	write_cmos_sensor_byte(0x6e47,0x19);
	write_cmos_sensor_byte(0x6e48,0xff);
	write_cmos_sensor_byte(0x6e49,0x19);
	write_cmos_sensor_byte(0x6e4a,0x19);
	write_cmos_sensor_byte(0x6e4b,0x19);
	write_cmos_sensor_byte(0x6e4c,0x19);
	write_cmos_sensor_byte(0x6e4d,0x19);
	write_cmos_sensor_byte(0x6e4e,0x19);
	write_cmos_sensor_byte(0x6e4f,0x19);
	write_cmos_sensor_byte(0x6e50,0x19);
	write_cmos_sensor_byte(0x6e51,0xff);
	write_cmos_sensor_byte(0x6e52,0x19);
	write_cmos_sensor_byte(0x6e53,0x19);
	write_cmos_sensor_byte(0x6e54,0x19);
	write_cmos_sensor_byte(0x6e55,0x19);
	write_cmos_sensor_byte(0x6e56,0x19);
	write_cmos_sensor_byte(0x6e57,0x19);
	write_cmos_sensor_byte(0x6e58,0x19);
	write_cmos_sensor_byte(0x6e59,0x19);
	write_cmos_sensor_byte(0x6e5a,0xf8);
	write_cmos_sensor_byte(0x6e5b,0x19);
	write_cmos_sensor_byte(0x6e5c,0x19);
	write_cmos_sensor_byte(0x6e5d,0x19);
	write_cmos_sensor_byte(0x6e5e,0x19);
	write_cmos_sensor_byte(0x6e5f,0x19);
	write_cmos_sensor_byte(0x6e60,0xff);
	write_cmos_sensor_byte(0x6e61,0x19);
	write_cmos_sensor_byte(0x6e62,0x19);
	write_cmos_sensor_byte(0x6e63,0x19);
	write_cmos_sensor_byte(0x6e64,0x19);
	write_cmos_sensor_byte(0x6e65,0x19);
	write_cmos_sensor_byte(0x6e66,0x19);
	write_cmos_sensor_byte(0x6e67,0x19);
	write_cmos_sensor_byte(0x6e68,0x19);
	write_cmos_sensor_byte(0x6e69,0xff);
	write_cmos_sensor_byte(0x6e6a,0x19);
	write_cmos_sensor_byte(0x6e6b,0x19);
	write_cmos_sensor_byte(0x6e6c,0x19);
	write_cmos_sensor_byte(0x6e6d,0x19);
	write_cmos_sensor_byte(0x6e6e,0x19);
	write_cmos_sensor_byte(0x6e6f,0x19);
	write_cmos_sensor_byte(0x6e70,0x19);
	write_cmos_sensor_byte(0x6e71,0x19);
	write_cmos_sensor_byte(0x6e72,0xff);
	write_cmos_sensor_byte(0x6e73,0x19);
	write_cmos_sensor_byte(0x6e74,0x19);
	write_cmos_sensor_byte(0x6e75,0x19);
	write_cmos_sensor_byte(0x6e76,0x19);
	write_cmos_sensor_byte(0x6e77,0x19);
	write_cmos_sensor_byte(0x6e78,0x19);
	write_cmos_sensor_byte(0x6e79,0x19);
	write_cmos_sensor_byte(0x6e7a,0x19);
	write_cmos_sensor_byte(0x6e7b,0xff);
	write_cmos_sensor_byte(0x6e7c,0x19);
	write_cmos_sensor_byte(0x6e7d,0x19);
	write_cmos_sensor_byte(0x6e7e,0x19);
	write_cmos_sensor_byte(0x6e7f,0x19);
	write_cmos_sensor_byte(0x6e80,0x19);
	write_cmos_sensor_byte(0x6e81,0x19);
	write_cmos_sensor_byte(0x6e82,0x19);
	write_cmos_sensor_byte(0x6e83,0x19);
	write_cmos_sensor_byte(0x6e84,0xff);
	write_cmos_sensor_byte(0x6e85,0x19);
	write_cmos_sensor_byte(0x6e86,0x19);
	write_cmos_sensor_byte(0x6e87,0x19);
	write_cmos_sensor_byte(0x6e88,0x19);
	write_cmos_sensor_byte(0x6e89,0x19);
	write_cmos_sensor_byte(0x6e8a,0x19);
	write_cmos_sensor_byte(0x6e8b,0x19);
	write_cmos_sensor_byte(0x6e8c,0x19);
	write_cmos_sensor_byte(0x6e8d,0xff);
	write_cmos_sensor_byte(0x6e8e,0x19);
	write_cmos_sensor_byte(0x6e8f,0x19);
	write_cmos_sensor_byte(0x6e90,0x19);
	write_cmos_sensor_byte(0x6e91,0x19);
	write_cmos_sensor_byte(0x6e92,0x19);
	write_cmos_sensor_byte(0x6e93,0x19);
	write_cmos_sensor_byte(0x6e94,0x19);
	write_cmos_sensor_byte(0x6e95,0x19);
	write_cmos_sensor_byte(0x6e96,0xff);
	write_cmos_sensor_byte(0x6e97,0x19);
	write_cmos_sensor_byte(0x6e98,0x19);
	write_cmos_sensor_byte(0x6e99,0x19);
	write_cmos_sensor_byte(0x6e9a,0x19);
	write_cmos_sensor_byte(0x6e9b,0x19);
	write_cmos_sensor_byte(0x6e9c,0x19);
	write_cmos_sensor_byte(0x6e9d,0x19);
	write_cmos_sensor_byte(0x6e9e,0x19);
	write_cmos_sensor_byte(0x6e9f,0xff);
	write_cmos_sensor_byte(0x6ea0,0x19);
	write_cmos_sensor_byte(0x6ea1,0x19);
	write_cmos_sensor_byte(0x6ea2,0x19);
	write_cmos_sensor_byte(0x6ea3,0x19);
	write_cmos_sensor_byte(0x6ea4,0x19);
	write_cmos_sensor_byte(0x6ea5,0x19);
	write_cmos_sensor_byte(0x6ea6,0x19);
	write_cmos_sensor_byte(0x6ea7,0x19);
	write_cmos_sensor_byte(0x6ea8,0xff);
	write_cmos_sensor_byte(0x6ea9,0x19);
	write_cmos_sensor_byte(0x6eaa,0x19);
	write_cmos_sensor_byte(0x6eab,0x19);
	write_cmos_sensor_byte(0x6eac,0x19);
	write_cmos_sensor_byte(0x6ead,0x19);
	write_cmos_sensor_byte(0x6eae,0x19);
	write_cmos_sensor_byte(0x6eaf,0x19);
	write_cmos_sensor_byte(0x6eb0,0x19);
	write_cmos_sensor_byte(0x6eb1,0xf8);
	write_cmos_sensor_byte(0x6eb2,0x19);
	write_cmos_sensor_byte(0x6eb3,0x19);
	write_cmos_sensor_byte(0x6eb4,0x19);
	write_cmos_sensor_byte(0x6eb5,0x19);
	write_cmos_sensor_byte(0x6eb6,0x19);
	write_cmos_sensor_byte(0x6ec1,0x00);
	write_cmos_sensor_byte(0x6ec2,0x21);
	write_cmos_sensor_byte(0x6ec3,0x0f);
	write_cmos_sensor_byte(0x6ec4,0xf0);
	write_cmos_sensor_byte(0x6ec5,0x00);
	write_cmos_sensor_byte(0x6ec6,0x21);
	write_cmos_sensor_byte(0x6ec7,0x0b);
	write_cmos_sensor_byte(0x6ec8,0xf0);
	write_cmos_sensor_byte(0x6ec9,0x03);
	write_cmos_sensor_byte(0x6eca,0xf4);
	write_cmos_sensor_byte(0x6ecb,0x02);
	write_cmos_sensor_byte(0x6ecc,0xf4);
	write_cmos_sensor_byte(0x6ece,0x10);
	write_cmos_sensor_byte(0x4853,0xb8);
	write_cmos_sensor_byte(0x4854,0xb0);
	write_cmos_sensor_byte(0x3650,0x51);
	write_cmos_sensor_byte(0x3651,0x9f);
	LOG_INF("preview_setting end\n");
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting start\n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x10);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x10);
	write_cmos_sensor_byte(0x3059,0x86);
	write_cmos_sensor_byte(0x305a,0x43);
	write_cmos_sensor_byte(0x305b,0x52);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3208,0x10);
	write_cmos_sensor_byte(0x3209,0x00);
	write_cmos_sensor_byte(0x320a,0x0c);
	write_cmos_sensor_byte(0x320b,0x00);
	write_cmos_sensor_byte(0x320c,0x04);
	write_cmos_sensor_byte(0x320d,0xd8);
	write_cmos_sensor_byte(0x320e,0x0c);
	write_cmos_sensor_byte(0x320f,0x98);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x30);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x22);
	write_cmos_sensor_byte(0x321a,0x10);
	write_cmos_sensor_byte(0x3220,0x02);
	write_cmos_sensor_byte(0x3225,0x21);
	write_cmos_sensor_byte(0x3226,0x05);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x20);
	write_cmos_sensor_byte(0x325f,0x40);
	write_cmos_sensor_byte(0x3280,0x0d);
	write_cmos_sensor_byte(0x3284,0x02);
	write_cmos_sensor_byte(0x3287,0x20);
	write_cmos_sensor_byte(0x329d,0x8c);
	write_cmos_sensor_byte(0x32d1,0x30);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x3309,0x60);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x331f,0x51);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x3399,0x0a);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x18);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x08);
	write_cmos_sensor_byte(0x34bb,0x01);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0xbc);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3636,0xc5);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x41);
	write_cmos_sensor_byte(0x3672,0x21);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x34);
	write_cmos_sensor_byte(0x3691,0x46);
	write_cmos_sensor_byte(0x3692,0x46);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0xb8);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x0b);
	write_cmos_sensor_byte(0x36c0,0x18);
	write_cmos_sensor_byte(0x36c1,0x18);
	write_cmos_sensor_byte(0x36c2,0x54);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xd3);
	write_cmos_sensor_byte(0x3771,0x1f);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x02);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0xc1);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x90);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3b09,0x00);
	write_cmos_sensor_byte(0x3b0a,0x00);
	write_cmos_sensor_byte(0x3b0b,0x00);
	write_cmos_sensor_byte(0x3b0c,0x00);
	write_cmos_sensor_byte(0x3b0d,0x10);
	write_cmos_sensor_byte(0x3b0e,0x10);
	write_cmos_sensor_byte(0x3b0f,0x10);
	write_cmos_sensor_byte(0x3b10,0x10);
	write_cmos_sensor_byte(0x3b11,0x20);
	write_cmos_sensor_byte(0x3b12,0x20);
	write_cmos_sensor_byte(0x3b13,0x20);
	write_cmos_sensor_byte(0x3b14,0x20);
	write_cmos_sensor_byte(0x3b15,0x30);
	write_cmos_sensor_byte(0x3b16,0x30);
	write_cmos_sensor_byte(0x3b17,0x30);
	write_cmos_sensor_byte(0x3b18,0x30);
	write_cmos_sensor_byte(0x3b49,0x03);
	write_cmos_sensor_byte(0x3b4c,0x03);
	write_cmos_sensor_byte(0x3b4d,0xff);
	write_cmos_sensor_byte(0x3b50,0x03);
	write_cmos_sensor_byte(0x3b51,0x7a);
	write_cmos_sensor_byte(0x3b52,0x03);
	write_cmos_sensor_byte(0x3b53,0x7b);
	write_cmos_sensor_byte(0x3b54,0x03);
	write_cmos_sensor_byte(0x3b55,0x80);
	write_cmos_sensor_byte(0x3b5a,0x03);
	write_cmos_sensor_byte(0x3b5b,0xf8);
	write_cmos_sensor_byte(0x3b5c,0x03);
	write_cmos_sensor_byte(0x3b5d,0xf9);
	write_cmos_sensor_byte(0x3b64,0x03);
	write_cmos_sensor_byte(0x3b65,0x7c);
	write_cmos_sensor_byte(0x3b66,0x03);
	write_cmos_sensor_byte(0x3b67,0x7b);
	write_cmos_sensor_byte(0x3b68,0x03);
	write_cmos_sensor_byte(0x3b69,0x80);
	write_cmos_sensor_byte(0x3b6e,0x03);
	write_cmos_sensor_byte(0x3b6f,0xf8);
	write_cmos_sensor_byte(0x3b70,0x03);
	write_cmos_sensor_byte(0x3b71,0xfc);
	write_cmos_sensor_byte(0x3c00,0x08);
	write_cmos_sensor_byte(0x3c06,0x07);
	write_cmos_sensor_byte(0x3c07,0x02);
	write_cmos_sensor_byte(0x3c2c,0x47);
	write_cmos_sensor_byte(0x3c65,0x05);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0xb1);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x3e25,0xff);
	write_cmos_sensor_byte(0x3e26,0xff);
	write_cmos_sensor_byte(0x3e29,0x08);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x38);
	write_cmos_sensor_byte(0x420f,0x78);
	write_cmos_sensor_byte(0x4210,0x12);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x08);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x10);
	write_cmos_sensor_byte(0x421d,0x10);
	write_cmos_sensor_byte(0x421e,0x12);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x43ac,0x80);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4503,0x2c);
	write_cmos_sensor_byte(0x4506,0xa2);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4512,0x01);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0x0f);
	write_cmos_sensor_byte(0x5002,0xd8);
	write_cmos_sensor_byte(0x5003,0xf0);
	write_cmos_sensor_byte(0x5004,0x00);
	write_cmos_sensor_byte(0x5009,0x0f);
	write_cmos_sensor_byte(0x5016,0x5a);
	write_cmos_sensor_byte(0x5017,0x5a);
	write_cmos_sensor_byte(0x5018,0x00);
	write_cmos_sensor_byte(0x5019,0x21);
	write_cmos_sensor_byte(0x501a,0x0f);
	write_cmos_sensor_byte(0x501b,0xf0);
	write_cmos_sensor_byte(0x501c,0x00);
	write_cmos_sensor_byte(0x501d,0x21);
	write_cmos_sensor_byte(0x501e,0x0b);
	write_cmos_sensor_byte(0x501f,0xf0);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5075,0xcd);
	write_cmos_sensor_byte(0x507f,0xe2);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x0c);
	write_cmos_sensor_byte(0x53c5,0x40);
	write_cmos_sensor_byte(0x5401,0x08);
	write_cmos_sensor_byte(0x540a,0x0c);
	write_cmos_sensor_byte(0x540b,0x5f);
	write_cmos_sensor_byte(0x540e,0x03);
	write_cmos_sensor_byte(0x540f,0x70);
	write_cmos_sensor_byte(0x5414,0x0c);
	write_cmos_sensor_byte(0x5415,0x80);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x541b,0x00);
	write_cmos_sensor_byte(0x541c,0x49);
	write_cmos_sensor_byte(0x541d,0x0c);
	write_cmos_sensor_byte(0x541e,0x18);
	write_cmos_sensor_byte(0x5501,0x08);
	write_cmos_sensor_byte(0x550a,0x0c);
	write_cmos_sensor_byte(0x550b,0x5f);
	write_cmos_sensor_byte(0x550e,0x02);
	write_cmos_sensor_byte(0x550f,0x00);
	write_cmos_sensor_byte(0x5514,0x0c);
	write_cmos_sensor_byte(0x5515,0x80);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x551b,0x00);
	write_cmos_sensor_byte(0x551c,0x49);
	write_cmos_sensor_byte(0x551d,0x0c);
	write_cmos_sensor_byte(0x551e,0x18);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x00);
	write_cmos_sensor_byte(0x5809,0x49);
	write_cmos_sensor_byte(0x580a,0x0c);
	write_cmos_sensor_byte(0x580b,0x18);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x00);
	write_cmos_sensor_byte(0x5827,0x00);
	write_cmos_sensor_byte(0x5828,0x0c);
	write_cmos_sensor_byte(0x5829,0x5f);
	write_cmos_sensor_byte(0x5f00,0x05);
	write_cmos_sensor_byte(0x5f06,0x00);
	write_cmos_sensor_byte(0x5f07,0x42);
	write_cmos_sensor_byte(0x5f08,0x1f);
	write_cmos_sensor_byte(0x5f09,0xe1);
	write_cmos_sensor_byte(0x5f0c,0x00);
	write_cmos_sensor_byte(0x5f0d,0x92);
	write_cmos_sensor_byte(0x5f0e,0x18);
	write_cmos_sensor_byte(0x5f0f,0x31);
	write_cmos_sensor_byte(0x5f16,0x00);
	write_cmos_sensor_byte(0x5f17,0x00);
	write_cmos_sensor_byte(0x5f18,0x20);
	write_cmos_sensor_byte(0x5f19,0x1f);
	write_cmos_sensor_byte(0x5f1a,0x00);
	write_cmos_sensor_byte(0x5f1b,0x00);
	write_cmos_sensor_byte(0x5f1c,0x18);
	write_cmos_sensor_byte(0x5f1d,0xbf);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x61b9,0xe0);
	write_cmos_sensor_byte(0x61bd,0x02);
	write_cmos_sensor_byte(0x61bf,0x00);
	write_cmos_sensor_byte(0x61e1,0x03);
	write_cmos_sensor_byte(0x61e2,0xf0);
	write_cmos_sensor_byte(0x61e3,0x02);
	write_cmos_sensor_byte(0x61e4,0xf4);
	write_cmos_sensor_byte(0x61e5,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
	write_cmos_sensor_byte(0x6e00,0x03);
	write_cmos_sensor_byte(0x6e05,0x66);
	write_cmos_sensor_byte(0x6e06,0x66);
	write_cmos_sensor_byte(0x6e07,0x66);
	write_cmos_sensor_byte(0x6e08,0x66);
	write_cmos_sensor_byte(0x6e09,0xff);
	write_cmos_sensor_byte(0x6e0a,0x19);
	write_cmos_sensor_byte(0x6e0b,0x19);
	write_cmos_sensor_byte(0x6e0c,0x19);
	write_cmos_sensor_byte(0x6e0d,0x19);
	write_cmos_sensor_byte(0x6e0e,0x19);
	write_cmos_sensor_byte(0x6e0f,0x19);
	write_cmos_sensor_byte(0x6e10,0x19);
	write_cmos_sensor_byte(0x6e11,0x19);
	write_cmos_sensor_byte(0x6e12,0xff);
	write_cmos_sensor_byte(0x6e13,0x19);
	write_cmos_sensor_byte(0x6e14,0x19);
	write_cmos_sensor_byte(0x6e15,0x19);
	write_cmos_sensor_byte(0x6e16,0x19);
	write_cmos_sensor_byte(0x6e17,0x19);
	write_cmos_sensor_byte(0x6e18,0x19);
	write_cmos_sensor_byte(0x6e19,0x19);
	write_cmos_sensor_byte(0x6e1a,0x19);
	write_cmos_sensor_byte(0x6e1b,0xff);
	write_cmos_sensor_byte(0x6e1c,0x19);
	write_cmos_sensor_byte(0x6e1d,0x19);
	write_cmos_sensor_byte(0x6e1e,0x19);
	write_cmos_sensor_byte(0x6e1f,0x19);
	write_cmos_sensor_byte(0x6e20,0x19);
	write_cmos_sensor_byte(0x6e21,0x19);
	write_cmos_sensor_byte(0x6e22,0x19);
	write_cmos_sensor_byte(0x6e23,0x19);
	write_cmos_sensor_byte(0x6e24,0xff);
	write_cmos_sensor_byte(0x6e25,0x19);
	write_cmos_sensor_byte(0x6e26,0x19);
	write_cmos_sensor_byte(0x6e27,0x19);
	write_cmos_sensor_byte(0x6e28,0x19);
	write_cmos_sensor_byte(0x6e29,0x19);
	write_cmos_sensor_byte(0x6e2a,0x19);
	write_cmos_sensor_byte(0x6e2b,0x19);
	write_cmos_sensor_byte(0x6e2c,0x19);
	write_cmos_sensor_byte(0x6e2d,0xff);
	write_cmos_sensor_byte(0x6e2e,0x19);
	write_cmos_sensor_byte(0x6e2f,0x19);
	write_cmos_sensor_byte(0x6e30,0x19);
	write_cmos_sensor_byte(0x6e31,0x19);
	write_cmos_sensor_byte(0x6e32,0x19);
	write_cmos_sensor_byte(0x6e33,0x19);
	write_cmos_sensor_byte(0x6e34,0x19);
	write_cmos_sensor_byte(0x6e35,0x19);
	write_cmos_sensor_byte(0x6e36,0xff);
	write_cmos_sensor_byte(0x6e37,0x19);
	write_cmos_sensor_byte(0x6e38,0x19);
	write_cmos_sensor_byte(0x6e39,0x19);
	write_cmos_sensor_byte(0x6e3a,0x19);
	write_cmos_sensor_byte(0x6e3b,0x19);
	write_cmos_sensor_byte(0x6e3c,0x19);
	write_cmos_sensor_byte(0x6e3d,0x19);
	write_cmos_sensor_byte(0x6e3e,0x19);
	write_cmos_sensor_byte(0x6e3f,0xff);
	write_cmos_sensor_byte(0x6e40,0x19);
	write_cmos_sensor_byte(0x6e41,0x19);
	write_cmos_sensor_byte(0x6e42,0x19);
	write_cmos_sensor_byte(0x6e43,0x19);
	write_cmos_sensor_byte(0x6e44,0x19);
	write_cmos_sensor_byte(0x6e45,0x19);
	write_cmos_sensor_byte(0x6e46,0x19);
	write_cmos_sensor_byte(0x6e47,0x19);
	write_cmos_sensor_byte(0x6e48,0xff);
	write_cmos_sensor_byte(0x6e49,0x19);
	write_cmos_sensor_byte(0x6e4a,0x19);
	write_cmos_sensor_byte(0x6e4b,0x19);
	write_cmos_sensor_byte(0x6e4c,0x19);
	write_cmos_sensor_byte(0x6e4d,0x19);
	write_cmos_sensor_byte(0x6e4e,0x19);
	write_cmos_sensor_byte(0x6e4f,0x19);
	write_cmos_sensor_byte(0x6e50,0x19);
	write_cmos_sensor_byte(0x6e51,0xff);
	write_cmos_sensor_byte(0x6e52,0x19);
	write_cmos_sensor_byte(0x6e53,0x19);
	write_cmos_sensor_byte(0x6e54,0x19);
	write_cmos_sensor_byte(0x6e55,0x19);
	write_cmos_sensor_byte(0x6e56,0x19);
	write_cmos_sensor_byte(0x6e57,0x19);
	write_cmos_sensor_byte(0x6e58,0x19);
	write_cmos_sensor_byte(0x6e59,0x19);
	write_cmos_sensor_byte(0x6e5a,0xf8);
	write_cmos_sensor_byte(0x6e5b,0x19);
	write_cmos_sensor_byte(0x6e5c,0x19);
	write_cmos_sensor_byte(0x6e5d,0x19);
	write_cmos_sensor_byte(0x6e5e,0x19);
	write_cmos_sensor_byte(0x6e5f,0x19);
	write_cmos_sensor_byte(0x6e60,0xff);
	write_cmos_sensor_byte(0x6e61,0x19);
	write_cmos_sensor_byte(0x6e62,0x19);
	write_cmos_sensor_byte(0x6e63,0x19);
	write_cmos_sensor_byte(0x6e64,0x19);
	write_cmos_sensor_byte(0x6e65,0x19);
	write_cmos_sensor_byte(0x6e66,0x19);
	write_cmos_sensor_byte(0x6e67,0x19);
	write_cmos_sensor_byte(0x6e68,0x19);
	write_cmos_sensor_byte(0x6e69,0xff);
	write_cmos_sensor_byte(0x6e6a,0x19);
	write_cmos_sensor_byte(0x6e6b,0x19);
	write_cmos_sensor_byte(0x6e6c,0x19);
	write_cmos_sensor_byte(0x6e6d,0x19);
	write_cmos_sensor_byte(0x6e6e,0x19);
	write_cmos_sensor_byte(0x6e6f,0x19);
	write_cmos_sensor_byte(0x6e70,0x19);
	write_cmos_sensor_byte(0x6e71,0x19);
	write_cmos_sensor_byte(0x6e72,0xff);
	write_cmos_sensor_byte(0x6e73,0x19);
	write_cmos_sensor_byte(0x6e74,0x19);
	write_cmos_sensor_byte(0x6e75,0x19);
	write_cmos_sensor_byte(0x6e76,0x19);
	write_cmos_sensor_byte(0x6e77,0x19);
	write_cmos_sensor_byte(0x6e78,0x19);
	write_cmos_sensor_byte(0x6e79,0x19);
	write_cmos_sensor_byte(0x6e7a,0x19);
	write_cmos_sensor_byte(0x6e7b,0xff);
	write_cmos_sensor_byte(0x6e7c,0x19);
	write_cmos_sensor_byte(0x6e7d,0x19);
	write_cmos_sensor_byte(0x6e7e,0x19);
	write_cmos_sensor_byte(0x6e7f,0x19);
	write_cmos_sensor_byte(0x6e80,0x19);
	write_cmos_sensor_byte(0x6e81,0x19);
	write_cmos_sensor_byte(0x6e82,0x19);
	write_cmos_sensor_byte(0x6e83,0x19);
	write_cmos_sensor_byte(0x6e84,0xff);
	write_cmos_sensor_byte(0x6e85,0x19);
	write_cmos_sensor_byte(0x6e86,0x19);
	write_cmos_sensor_byte(0x6e87,0x19);
	write_cmos_sensor_byte(0x6e88,0x19);
	write_cmos_sensor_byte(0x6e89,0x19);
	write_cmos_sensor_byte(0x6e8a,0x19);
	write_cmos_sensor_byte(0x6e8b,0x19);
	write_cmos_sensor_byte(0x6e8c,0x19);
	write_cmos_sensor_byte(0x6e8d,0xff);
	write_cmos_sensor_byte(0x6e8e,0x19);
	write_cmos_sensor_byte(0x6e8f,0x19);
	write_cmos_sensor_byte(0x6e90,0x19);
	write_cmos_sensor_byte(0x6e91,0x19);
	write_cmos_sensor_byte(0x6e92,0x19);
	write_cmos_sensor_byte(0x6e93,0x19);
	write_cmos_sensor_byte(0x6e94,0x19);
	write_cmos_sensor_byte(0x6e95,0x19);
	write_cmos_sensor_byte(0x6e96,0xff);
	write_cmos_sensor_byte(0x6e97,0x19);
	write_cmos_sensor_byte(0x6e98,0x19);
	write_cmos_sensor_byte(0x6e99,0x19);
	write_cmos_sensor_byte(0x6e9a,0x19);
	write_cmos_sensor_byte(0x6e9b,0x19);
	write_cmos_sensor_byte(0x6e9c,0x19);
	write_cmos_sensor_byte(0x6e9d,0x19);
	write_cmos_sensor_byte(0x6e9e,0x19);
	write_cmos_sensor_byte(0x6e9f,0xff);
	write_cmos_sensor_byte(0x6ea0,0x19);
	write_cmos_sensor_byte(0x6ea1,0x19);
	write_cmos_sensor_byte(0x6ea2,0x19);
	write_cmos_sensor_byte(0x6ea3,0x19);
	write_cmos_sensor_byte(0x6ea4,0x19);
	write_cmos_sensor_byte(0x6ea5,0x19);
	write_cmos_sensor_byte(0x6ea6,0x19);
	write_cmos_sensor_byte(0x6ea7,0x19);
	write_cmos_sensor_byte(0x6ea8,0xff);
	write_cmos_sensor_byte(0x6ea9,0x19);
	write_cmos_sensor_byte(0x6eaa,0x19);
	write_cmos_sensor_byte(0x6eab,0x19);
	write_cmos_sensor_byte(0x6eac,0x19);
	write_cmos_sensor_byte(0x6ead,0x19);
	write_cmos_sensor_byte(0x6eae,0x19);
	write_cmos_sensor_byte(0x6eaf,0x19);
	write_cmos_sensor_byte(0x6eb0,0x19);
	write_cmos_sensor_byte(0x6eb1,0xf8);
	write_cmos_sensor_byte(0x6eb2,0x19);
	write_cmos_sensor_byte(0x6eb3,0x19);
	write_cmos_sensor_byte(0x6eb4,0x19);
	write_cmos_sensor_byte(0x6eb5,0x19);
	write_cmos_sensor_byte(0x6eb6,0x19);
	write_cmos_sensor_byte(0x6ec1,0x00);
	write_cmos_sensor_byte(0x6ec2,0x21);
	write_cmos_sensor_byte(0x6ec3,0x0f);
	write_cmos_sensor_byte(0x6ec4,0xf0);
	write_cmos_sensor_byte(0x6ec5,0x00);
	write_cmos_sensor_byte(0x6ec6,0x21);
	write_cmos_sensor_byte(0x6ec7,0x0b);
	write_cmos_sensor_byte(0x6ec8,0xf0);
	write_cmos_sensor_byte(0x6ec9,0x03);
	write_cmos_sensor_byte(0x6eca,0xf4);
	write_cmos_sensor_byte(0x6ecb,0x02);
	write_cmos_sensor_byte(0x6ecc,0xf4);
	write_cmos_sensor_byte(0x6ece,0x10);
	write_cmos_sensor_byte(0x4853,0xb8);
	write_cmos_sensor_byte(0x4854,0xb0);
}



static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting start\n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x13);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x10);
	write_cmos_sensor_byte(0x3059,0x86);
	write_cmos_sensor_byte(0x305a,0x43);
	write_cmos_sensor_byte(0x305b,0x52);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3202,0x03);
	write_cmos_sensor_byte(0x3203,0x00);
	write_cmos_sensor_byte(0x3204,0x20);
	write_cmos_sensor_byte(0x3205,0x1f);
	write_cmos_sensor_byte(0x3206,0x15);
	write_cmos_sensor_byte(0x3207,0x1f);
	write_cmos_sensor_byte(0x3208,0x10);
	write_cmos_sensor_byte(0x3209,0x00);
	write_cmos_sensor_byte(0x320a,0x09);
	write_cmos_sensor_byte(0x320b,0x00);
	write_cmos_sensor_byte(0x320c,0x04);
	write_cmos_sensor_byte(0x320d,0xd8);
	write_cmos_sensor_byte(0x320e,0x0c);
	write_cmos_sensor_byte(0x320f,0x98);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x30);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x22);
	write_cmos_sensor_byte(0x321a,0x10);
	write_cmos_sensor_byte(0x3220,0x02);
	write_cmos_sensor_byte(0x3225,0x21);
	write_cmos_sensor_byte(0x3226,0x05);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x20);
	write_cmos_sensor_byte(0x325f,0x40);
	write_cmos_sensor_byte(0x3280,0x0d);
	write_cmos_sensor_byte(0x3284,0x02);
	write_cmos_sensor_byte(0x3287,0x20);
	write_cmos_sensor_byte(0x329d,0x8c);
	write_cmos_sensor_byte(0x32d1,0x30);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x3309,0x60);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x331f,0x51);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x3399,0x0a);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x18);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x08);
	write_cmos_sensor_byte(0x34bb,0x01);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0xbc);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3636,0xc5);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x41);
	write_cmos_sensor_byte(0x3672,0x21);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x34);
	write_cmos_sensor_byte(0x3691,0x46);
	write_cmos_sensor_byte(0x3692,0x46);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0xb8);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x0b);
	write_cmos_sensor_byte(0x36c0,0x18);
	write_cmos_sensor_byte(0x36c1,0x18);
	write_cmos_sensor_byte(0x36c2,0x54);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xd3);
	write_cmos_sensor_byte(0x3771,0x1f);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x02);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0xc1);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x90);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3b09,0x00);
	write_cmos_sensor_byte(0x3b0a,0x00);
	write_cmos_sensor_byte(0x3b0b,0x00);
	write_cmos_sensor_byte(0x3b0c,0x00);
	write_cmos_sensor_byte(0x3b0d,0x10);
	write_cmos_sensor_byte(0x3b0e,0x10);
	write_cmos_sensor_byte(0x3b0f,0x10);
	write_cmos_sensor_byte(0x3b10,0x10);
	write_cmos_sensor_byte(0x3b11,0x20);
	write_cmos_sensor_byte(0x3b12,0x20);
	write_cmos_sensor_byte(0x3b13,0x20);
	write_cmos_sensor_byte(0x3b14,0x20);
	write_cmos_sensor_byte(0x3b15,0x30);
	write_cmos_sensor_byte(0x3b16,0x30);
	write_cmos_sensor_byte(0x3b17,0x30);
	write_cmos_sensor_byte(0x3b18,0x30);
	write_cmos_sensor_byte(0x3b49,0x03);
	write_cmos_sensor_byte(0x3b4c,0x03);
	write_cmos_sensor_byte(0x3b4d,0xff);
	write_cmos_sensor_byte(0x3b50,0x03);
	write_cmos_sensor_byte(0x3b51,0x7a);
	write_cmos_sensor_byte(0x3b52,0x03);
	write_cmos_sensor_byte(0x3b53,0x7b);
	write_cmos_sensor_byte(0x3b54,0x03);
	write_cmos_sensor_byte(0x3b55,0x80);
	write_cmos_sensor_byte(0x3b5a,0x03);
	write_cmos_sensor_byte(0x3b5b,0xf8);
	write_cmos_sensor_byte(0x3b5c,0x03);
	write_cmos_sensor_byte(0x3b5d,0xf9);
	write_cmos_sensor_byte(0x3b64,0x03);
	write_cmos_sensor_byte(0x3b65,0x7c);
	write_cmos_sensor_byte(0x3b66,0x03);
	write_cmos_sensor_byte(0x3b67,0x7b);
	write_cmos_sensor_byte(0x3b68,0x03);
	write_cmos_sensor_byte(0x3b69,0x80);
	write_cmos_sensor_byte(0x3b6e,0x03);
	write_cmos_sensor_byte(0x3b6f,0xf8);
	write_cmos_sensor_byte(0x3b70,0x03);
	write_cmos_sensor_byte(0x3b71,0xfc);
	write_cmos_sensor_byte(0x3c00,0x08);
	write_cmos_sensor_byte(0x3c06,0x07);
	write_cmos_sensor_byte(0x3c07,0x02);
	write_cmos_sensor_byte(0x3c2c,0x47);
	write_cmos_sensor_byte(0x3c65,0x05);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0xb1);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x3e25,0xff);
	write_cmos_sensor_byte(0x3e26,0xff);
	write_cmos_sensor_byte(0x3e29,0x08);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x38);
	write_cmos_sensor_byte(0x420f,0x78);
	write_cmos_sensor_byte(0x4210,0x12);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x08);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x10);
	write_cmos_sensor_byte(0x421d,0x10);
	write_cmos_sensor_byte(0x421e,0x12);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x43ac,0x80);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4503,0x2c);
	write_cmos_sensor_byte(0x4506,0xa2);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4512,0x01);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0x0f);
	write_cmos_sensor_byte(0x5002,0xd8);
	write_cmos_sensor_byte(0x5003,0xf0);
	write_cmos_sensor_byte(0x5004,0x00);
	write_cmos_sensor_byte(0x5009,0x0f);
	write_cmos_sensor_byte(0x5016,0x5a);
	write_cmos_sensor_byte(0x5017,0x5a);
	write_cmos_sensor_byte(0x5018,0x00);
	write_cmos_sensor_byte(0x5019,0x21);
	write_cmos_sensor_byte(0x501a,0x0f);
	write_cmos_sensor_byte(0x501b,0xf0);
	write_cmos_sensor_byte(0x501c,0x00);
	write_cmos_sensor_byte(0x501d,0x21);
	write_cmos_sensor_byte(0x501e,0x0b);
	write_cmos_sensor_byte(0x501f,0xf0);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5075,0xcd);
	write_cmos_sensor_byte(0x507f,0xe2);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x0c);
	write_cmos_sensor_byte(0x53c5,0x40);
	write_cmos_sensor_byte(0x5401,0x08);
	write_cmos_sensor_byte(0x540a,0x0a);
	write_cmos_sensor_byte(0x540b,0xdf);
	write_cmos_sensor_byte(0x540e,0x03);
	write_cmos_sensor_byte(0x540f,0x70);
	write_cmos_sensor_byte(0x5414,0x0c);
	write_cmos_sensor_byte(0x5415,0x80);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x541b,0x01);
	write_cmos_sensor_byte(0x541c,0xa9);
	write_cmos_sensor_byte(0x541d,0x0a);
	write_cmos_sensor_byte(0x541e,0xb8);
	write_cmos_sensor_byte(0x5501,0x08);
	write_cmos_sensor_byte(0x550a,0x0a);
	write_cmos_sensor_byte(0x550b,0xdf);
	write_cmos_sensor_byte(0x550e,0x02);
	write_cmos_sensor_byte(0x550f,0x00);
	write_cmos_sensor_byte(0x5514,0x0c);
	write_cmos_sensor_byte(0x5515,0x80);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x551b,0x01);
	write_cmos_sensor_byte(0x551c,0xa9);
	write_cmos_sensor_byte(0x551d,0x0a);
	write_cmos_sensor_byte(0x551e,0xb8);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x01);
	write_cmos_sensor_byte(0x5809,0xa9);
	write_cmos_sensor_byte(0x580a,0x0a);
	write_cmos_sensor_byte(0x580b,0xb8);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x01);
	write_cmos_sensor_byte(0x5827,0x80);
	write_cmos_sensor_byte(0x5828,0x0a);
	write_cmos_sensor_byte(0x5829,0xdf);
	write_cmos_sensor_byte(0x5f00,0x05);
	write_cmos_sensor_byte(0x5f06,0x00);
	write_cmos_sensor_byte(0x5f07,0x42);
	write_cmos_sensor_byte(0x5f08,0x1f);
	write_cmos_sensor_byte(0x5f09,0xe1);
	write_cmos_sensor_byte(0x5f0c,0x03);
	write_cmos_sensor_byte(0x5f0d,0x52);
	write_cmos_sensor_byte(0x5f0e,0x15);
	write_cmos_sensor_byte(0x5f0f,0x71);
	write_cmos_sensor_byte(0x5f16,0x00);
	write_cmos_sensor_byte(0x5f17,0x00);
	write_cmos_sensor_byte(0x5f18,0x20);
	write_cmos_sensor_byte(0x5f19,0x1f);
	write_cmos_sensor_byte(0x5f1a,0x03);
	write_cmos_sensor_byte(0x5f1b,0x00);
	write_cmos_sensor_byte(0x5f1c,0x15);
	write_cmos_sensor_byte(0x5f1d,0xbf);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x61b9,0xe0);
	write_cmos_sensor_byte(0x61bd,0x02);
	write_cmos_sensor_byte(0x61bf,0x02);
	write_cmos_sensor_byte(0x61e1,0x03);
	write_cmos_sensor_byte(0x61e2,0xf0);
	write_cmos_sensor_byte(0x61e3,0x02);
	write_cmos_sensor_byte(0x61e4,0xf4);
	write_cmos_sensor_byte(0x61e5,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
	write_cmos_sensor_byte(0x6e00,0x03);
	write_cmos_sensor_byte(0x6e05,0x66);
	write_cmos_sensor_byte(0x6e06,0x66);
	write_cmos_sensor_byte(0x6e07,0x66);
	write_cmos_sensor_byte(0x6e08,0x66);
	write_cmos_sensor_byte(0x6e09,0xff);
	write_cmos_sensor_byte(0x6e0a,0x19);
	write_cmos_sensor_byte(0x6e0b,0x19);
	write_cmos_sensor_byte(0x6e0c,0x19);
	write_cmos_sensor_byte(0x6e0d,0x19);
	write_cmos_sensor_byte(0x6e0e,0x19);
	write_cmos_sensor_byte(0x6e0f,0x19);
	write_cmos_sensor_byte(0x6e10,0x19);
	write_cmos_sensor_byte(0x6e11,0x19);
	write_cmos_sensor_byte(0x6e12,0xff);
	write_cmos_sensor_byte(0x6e13,0x19);
	write_cmos_sensor_byte(0x6e14,0x19);
	write_cmos_sensor_byte(0x6e15,0x19);
	write_cmos_sensor_byte(0x6e16,0x19);
	write_cmos_sensor_byte(0x6e17,0x19);
	write_cmos_sensor_byte(0x6e18,0x19);
	write_cmos_sensor_byte(0x6e19,0x19);
	write_cmos_sensor_byte(0x6e1a,0x19);
	write_cmos_sensor_byte(0x6e1b,0xff);
	write_cmos_sensor_byte(0x6e1c,0x19);
	write_cmos_sensor_byte(0x6e1d,0x19);
	write_cmos_sensor_byte(0x6e1e,0x19);
	write_cmos_sensor_byte(0x6e1f,0x19);
	write_cmos_sensor_byte(0x6e20,0x19);
	write_cmos_sensor_byte(0x6e21,0x19);
	write_cmos_sensor_byte(0x6e22,0x19);
	write_cmos_sensor_byte(0x6e23,0x19);
	write_cmos_sensor_byte(0x6e24,0xff);
	write_cmos_sensor_byte(0x6e25,0x19);
	write_cmos_sensor_byte(0x6e26,0x19);
	write_cmos_sensor_byte(0x6e27,0x19);
	write_cmos_sensor_byte(0x6e28,0x19);
	write_cmos_sensor_byte(0x6e29,0x19);
	write_cmos_sensor_byte(0x6e2a,0x19);
	write_cmos_sensor_byte(0x6e2b,0x19);
	write_cmos_sensor_byte(0x6e2c,0x19);
	write_cmos_sensor_byte(0x6e2d,0xff);
	write_cmos_sensor_byte(0x6e2e,0x19);
	write_cmos_sensor_byte(0x6e2f,0x19);
	write_cmos_sensor_byte(0x6e30,0x19);
	write_cmos_sensor_byte(0x6e31,0x19);
	write_cmos_sensor_byte(0x6e32,0x19);
	write_cmos_sensor_byte(0x6e33,0x19);
	write_cmos_sensor_byte(0x6e34,0x19);
	write_cmos_sensor_byte(0x6e35,0x19);
	write_cmos_sensor_byte(0x6e36,0xff);
	write_cmos_sensor_byte(0x6e37,0x19);
	write_cmos_sensor_byte(0x6e38,0x19);
	write_cmos_sensor_byte(0x6e39,0x19);
	write_cmos_sensor_byte(0x6e3a,0x19);
	write_cmos_sensor_byte(0x6e3b,0x19);
	write_cmos_sensor_byte(0x6e3c,0x19);
	write_cmos_sensor_byte(0x6e3d,0x19);
	write_cmos_sensor_byte(0x6e3e,0x19);
	write_cmos_sensor_byte(0x6e3f,0xff);
	write_cmos_sensor_byte(0x6e40,0x19);
	write_cmos_sensor_byte(0x6e41,0x19);
	write_cmos_sensor_byte(0x6e42,0x19);
	write_cmos_sensor_byte(0x6e43,0x19);
	write_cmos_sensor_byte(0x6e44,0x19);
	write_cmos_sensor_byte(0x6e45,0x19);
	write_cmos_sensor_byte(0x6e46,0x19);
	write_cmos_sensor_byte(0x6e47,0x19);
	write_cmos_sensor_byte(0x6e48,0xff);
	write_cmos_sensor_byte(0x6e49,0x19);
	write_cmos_sensor_byte(0x6e4a,0x19);
	write_cmos_sensor_byte(0x6e4b,0x19);
	write_cmos_sensor_byte(0x6e4c,0x19);
	write_cmos_sensor_byte(0x6e4d,0x19);
	write_cmos_sensor_byte(0x6e4e,0x19);
	write_cmos_sensor_byte(0x6e4f,0x19);
	write_cmos_sensor_byte(0x6e50,0x19);
	write_cmos_sensor_byte(0x6e51,0xff);
	write_cmos_sensor_byte(0x6e52,0x19);
	write_cmos_sensor_byte(0x6e53,0x19);
	write_cmos_sensor_byte(0x6e54,0x19);
	write_cmos_sensor_byte(0x6e55,0x19);
	write_cmos_sensor_byte(0x6e56,0x19);
	write_cmos_sensor_byte(0x6e57,0x19);
	write_cmos_sensor_byte(0x6e58,0x19);
	write_cmos_sensor_byte(0x6e59,0x19);
	write_cmos_sensor_byte(0x6e5a,0xf8);
	write_cmos_sensor_byte(0x6e5b,0x19);
	write_cmos_sensor_byte(0x6e5c,0x19);
	write_cmos_sensor_byte(0x6e5d,0x19);
	write_cmos_sensor_byte(0x6e5e,0x19);
	write_cmos_sensor_byte(0x6e5f,0x19);
	write_cmos_sensor_byte(0x6e60,0xff);
	write_cmos_sensor_byte(0x6e61,0x19);
	write_cmos_sensor_byte(0x6e62,0x19);
	write_cmos_sensor_byte(0x6e63,0x19);
	write_cmos_sensor_byte(0x6e64,0x19);
	write_cmos_sensor_byte(0x6e65,0x19);
	write_cmos_sensor_byte(0x6e66,0x19);
	write_cmos_sensor_byte(0x6e67,0x19);
	write_cmos_sensor_byte(0x6e68,0x19);
	write_cmos_sensor_byte(0x6e69,0xff);
	write_cmos_sensor_byte(0x6e6a,0x19);
	write_cmos_sensor_byte(0x6e6b,0x19);
	write_cmos_sensor_byte(0x6e6c,0x19);
	write_cmos_sensor_byte(0x6e6d,0x19);
	write_cmos_sensor_byte(0x6e6e,0x19);
	write_cmos_sensor_byte(0x6e6f,0x19);
	write_cmos_sensor_byte(0x6e70,0x19);
	write_cmos_sensor_byte(0x6e71,0x19);
	write_cmos_sensor_byte(0x6e72,0xff);
	write_cmos_sensor_byte(0x6e73,0x19);
	write_cmos_sensor_byte(0x6e74,0x19);
	write_cmos_sensor_byte(0x6e75,0x19);
	write_cmos_sensor_byte(0x6e76,0x19);
	write_cmos_sensor_byte(0x6e77,0x19);
	write_cmos_sensor_byte(0x6e78,0x19);
	write_cmos_sensor_byte(0x6e79,0x19);
	write_cmos_sensor_byte(0x6e7a,0x19);
	write_cmos_sensor_byte(0x6e7b,0xff);
	write_cmos_sensor_byte(0x6e7c,0x19);
	write_cmos_sensor_byte(0x6e7d,0x19);
	write_cmos_sensor_byte(0x6e7e,0x19);
	write_cmos_sensor_byte(0x6e7f,0x19);
	write_cmos_sensor_byte(0x6e80,0x19);
	write_cmos_sensor_byte(0x6e81,0x19);
	write_cmos_sensor_byte(0x6e82,0x19);
	write_cmos_sensor_byte(0x6e83,0x19);
	write_cmos_sensor_byte(0x6e84,0xff);
	write_cmos_sensor_byte(0x6e85,0x19);
	write_cmos_sensor_byte(0x6e86,0x19);
	write_cmos_sensor_byte(0x6e87,0x19);
	write_cmos_sensor_byte(0x6e88,0x19);
	write_cmos_sensor_byte(0x6e89,0x19);
	write_cmos_sensor_byte(0x6e8a,0x19);
	write_cmos_sensor_byte(0x6e8b,0x19);
	write_cmos_sensor_byte(0x6e8c,0x19);
	write_cmos_sensor_byte(0x6e8d,0xff);
	write_cmos_sensor_byte(0x6e8e,0x19);
	write_cmos_sensor_byte(0x6e8f,0x19);
	write_cmos_sensor_byte(0x6e90,0x19);
	write_cmos_sensor_byte(0x6e91,0x19);
	write_cmos_sensor_byte(0x6e92,0x19);
	write_cmos_sensor_byte(0x6e93,0x19);
	write_cmos_sensor_byte(0x6e94,0x19);
	write_cmos_sensor_byte(0x6e95,0x19);
	write_cmos_sensor_byte(0x6e96,0xff);
	write_cmos_sensor_byte(0x6e97,0x19);
	write_cmos_sensor_byte(0x6e98,0x19);
	write_cmos_sensor_byte(0x6e99,0x19);
	write_cmos_sensor_byte(0x6e9a,0x19);
	write_cmos_sensor_byte(0x6e9b,0x19);
	write_cmos_sensor_byte(0x6e9c,0x19);
	write_cmos_sensor_byte(0x6e9d,0x19);
	write_cmos_sensor_byte(0x6e9e,0x19);
	write_cmos_sensor_byte(0x6e9f,0xff);
	write_cmos_sensor_byte(0x6ea0,0x19);
	write_cmos_sensor_byte(0x6ea1,0x19);
	write_cmos_sensor_byte(0x6ea2,0x19);
	write_cmos_sensor_byte(0x6ea3,0x19);
	write_cmos_sensor_byte(0x6ea4,0x19);
	write_cmos_sensor_byte(0x6ea5,0x19);
	write_cmos_sensor_byte(0x6ea6,0x19);
	write_cmos_sensor_byte(0x6ea7,0x19);
	write_cmos_sensor_byte(0x6ea8,0xff);
	write_cmos_sensor_byte(0x6ea9,0x19);
	write_cmos_sensor_byte(0x6eaa,0x19);
	write_cmos_sensor_byte(0x6eab,0x19);
	write_cmos_sensor_byte(0x6eac,0x19);
	write_cmos_sensor_byte(0x6ead,0x19);
	write_cmos_sensor_byte(0x6eae,0x19);
	write_cmos_sensor_byte(0x6eaf,0x19);
	write_cmos_sensor_byte(0x6eb0,0x19);
	write_cmos_sensor_byte(0x6eb1,0xf8);
	write_cmos_sensor_byte(0x6eb2,0x19);
	write_cmos_sensor_byte(0x6eb3,0x19);
	write_cmos_sensor_byte(0x6eb4,0x19);
	write_cmos_sensor_byte(0x6eb5,0x19);
	write_cmos_sensor_byte(0x6eb6,0x19);
	write_cmos_sensor_byte(0x6ec1,0x00);
	write_cmos_sensor_byte(0x6ec2,0x21);
	write_cmos_sensor_byte(0x6ec3,0x0f);
	write_cmos_sensor_byte(0x6ec4,0xf0);
	write_cmos_sensor_byte(0x6ec5,0x00);
	write_cmos_sensor_byte(0x6ec6,0x21);
	write_cmos_sensor_byte(0x6ec7,0x0b);
	write_cmos_sensor_byte(0x6ec8,0xf0);
	write_cmos_sensor_byte(0x6ec9,0x03);
	write_cmos_sensor_byte(0x6eca,0xf4);
	write_cmos_sensor_byte(0x6ecb,0x02);
	write_cmos_sensor_byte(0x6ecc,0xf4);
	write_cmos_sensor_byte(0x6ece,0x10);
	write_cmos_sensor_byte(0x4853,0xb8);
	write_cmos_sensor_byte(0x4854,0xb0);
}

static void hs_video_setting(void) 
{
	LOG_INF("hs_video_setting \n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x14);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x32);
	write_cmos_sensor_byte(0x3059,0x64);
	write_cmos_sensor_byte(0x305a,0x51);
	write_cmos_sensor_byte(0x305b,0x80);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3200,0x00);
	write_cmos_sensor_byte(0x3201,0xf0);
	write_cmos_sensor_byte(0x3202,0x03);
	write_cmos_sensor_byte(0x3203,0x80);
	write_cmos_sensor_byte(0x3204,0x1f);
	write_cmos_sensor_byte(0x3205,0x2f);
	write_cmos_sensor_byte(0x3206,0x14);
	write_cmos_sensor_byte(0x3207,0x9f);
	write_cmos_sensor_byte(0x3208,0x07);
	write_cmos_sensor_byte(0x3209,0x80);
	write_cmos_sensor_byte(0x320a,0x04);
	write_cmos_sensor_byte(0x320b,0x38);
	write_cmos_sensor_byte(0x320c,0x02);
	write_cmos_sensor_byte(0x320d,0x6c);
	write_cmos_sensor_byte(0x320e,0x06);
	write_cmos_sensor_byte(0x320f,0x4a);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x1c);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x62);
	write_cmos_sensor_byte(0x3220,0x20);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x10);
	write_cmos_sensor_byte(0x325f,0x30);
	write_cmos_sensor_byte(0x3280,0x0c);
	write_cmos_sensor_byte(0x3285,0x0a);
	write_cmos_sensor_byte(0x32d1,0x20);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334b,0x14);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x336f,0x50);
	write_cmos_sensor_byte(0x3375,0x51);
	write_cmos_sensor_byte(0x337e,0xb1);
	write_cmos_sensor_byte(0x3399,0x08);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x20);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x00);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0x9b);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x90);
	write_cmos_sensor_byte(0x3672,0x25);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x33);
	write_cmos_sensor_byte(0x3691,0x45);
	write_cmos_sensor_byte(0x3692,0x45);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0x98);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x01);
	write_cmos_sensor_byte(0x36c0,0x0b);
	write_cmos_sensor_byte(0x36c1,0x13);
	write_cmos_sensor_byte(0x36c2,0x34);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xdb);
	write_cmos_sensor_byte(0x3771,0x1b);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x00);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3906,0x0f);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0x41);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x70);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0x31);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x18);
	write_cmos_sensor_byte(0x420f,0x18);
	write_cmos_sensor_byte(0x4210,0x10);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x10);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x0a);
	write_cmos_sensor_byte(0x421d,0x0c);
	write_cmos_sensor_byte(0x421e,0x10);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0xdf);
	write_cmos_sensor_byte(0x5015,0x00);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x04);
	write_cmos_sensor_byte(0x5400,0x20);
	write_cmos_sensor_byte(0x5401,0xff);
	write_cmos_sensor_byte(0x5402,0x0f);
	write_cmos_sensor_byte(0x5403,0x20);
	write_cmos_sensor_byte(0x5404,0x00);
	write_cmos_sensor_byte(0x5405,0x78);
	write_cmos_sensor_byte(0x5406,0x0f);
	write_cmos_sensor_byte(0x5407,0x97);
	write_cmos_sensor_byte(0x5408,0x00);
	write_cmos_sensor_byte(0x5409,0xe0);
	write_cmos_sensor_byte(0x540a,0x05);
	write_cmos_sensor_byte(0x540b,0x27);
	write_cmos_sensor_byte(0x540c,0x11);
	write_cmos_sensor_byte(0x540d,0x11);
	write_cmos_sensor_byte(0x540e,0x01);
	write_cmos_sensor_byte(0x540f,0xbc);
	write_cmos_sensor_byte(0x5414,0x04);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x5500,0x20);
	write_cmos_sensor_byte(0x5501,0xff);
	write_cmos_sensor_byte(0x5502,0x0f);
	write_cmos_sensor_byte(0x5503,0x20);
	write_cmos_sensor_byte(0x5504,0x00);
	write_cmos_sensor_byte(0x5505,0x78);
	write_cmos_sensor_byte(0x5506,0x0f);
	write_cmos_sensor_byte(0x5507,0x97);
	write_cmos_sensor_byte(0x5508,0x00);
	write_cmos_sensor_byte(0x5509,0xe0);
	write_cmos_sensor_byte(0x550a,0x05);
	write_cmos_sensor_byte(0x550b,0x27);
	write_cmos_sensor_byte(0x550c,0x11);
	write_cmos_sensor_byte(0x550d,0x11);
	write_cmos_sensor_byte(0x550e,0x01);
	write_cmos_sensor_byte(0x550f,0x78);
	write_cmos_sensor_byte(0x5514,0x04);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x00);
	write_cmos_sensor_byte(0x5809,0x49);
	write_cmos_sensor_byte(0x580a,0x0c);
	write_cmos_sensor_byte(0x580b,0x18);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x00);
	write_cmos_sensor_byte(0x5827,0x00);
	write_cmos_sensor_byte(0x5828,0x0c);
	write_cmos_sensor_byte(0x5829,0x5f);
	write_cmos_sensor_byte(0x5900,0x01);
	write_cmos_sensor_byte(0x5901,0x84);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
}



static void slim_video_setting(void) 
{
	LOG_INF("slim_video_setting \n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x15);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x32);
	write_cmos_sensor_byte(0x3059,0x64);
	write_cmos_sensor_byte(0x305a,0x51);
	write_cmos_sensor_byte(0x305b,0x80);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3200,0x00);
	write_cmos_sensor_byte(0x3201,0xf0);
	write_cmos_sensor_byte(0x3202,0x03);
	write_cmos_sensor_byte(0x3203,0x80);
	write_cmos_sensor_byte(0x3204,0x1f);
	write_cmos_sensor_byte(0x3205,0x2f);
	write_cmos_sensor_byte(0x3206,0x14);
	write_cmos_sensor_byte(0x3207,0x9f);
	write_cmos_sensor_byte(0x3208,0x07);
	write_cmos_sensor_byte(0x3209,0x80);
	write_cmos_sensor_byte(0x320a,0x04);
	write_cmos_sensor_byte(0x320b,0x38);
	write_cmos_sensor_byte(0x320c,0x02);
	write_cmos_sensor_byte(0x320d,0x6c);
	write_cmos_sensor_byte(0x320e,0x0c);
	write_cmos_sensor_byte(0x320f,0xa0);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x1c);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x62);
	write_cmos_sensor_byte(0x3220,0x20);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x10);
	write_cmos_sensor_byte(0x325f,0x30);
	write_cmos_sensor_byte(0x3280,0x0c);
	write_cmos_sensor_byte(0x3285,0x0a);
	write_cmos_sensor_byte(0x32d1,0x20);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334b,0x14);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x336f,0x50);
	write_cmos_sensor_byte(0x3375,0x51);
	write_cmos_sensor_byte(0x337e,0xb1);
	write_cmos_sensor_byte(0x3399,0x08);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x20);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x00);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0x9b);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x90);
	write_cmos_sensor_byte(0x3672,0x25);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x33);
	write_cmos_sensor_byte(0x3691,0x45);
	write_cmos_sensor_byte(0x3692,0x45);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0x98);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x01);
	write_cmos_sensor_byte(0x36c0,0x0b);
	write_cmos_sensor_byte(0x36c1,0x13);
	write_cmos_sensor_byte(0x36c2,0x34);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xdb);
	write_cmos_sensor_byte(0x3771,0x1b);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x00);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3906,0x0f);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0x41);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x70);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0x31);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x18);
	write_cmos_sensor_byte(0x420f,0x18);
	write_cmos_sensor_byte(0x4210,0x10);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x10);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x0a);
	write_cmos_sensor_byte(0x421d,0x0c);
	write_cmos_sensor_byte(0x421e,0x10);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0xdf);
	write_cmos_sensor_byte(0x5015,0x00);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x04);
	write_cmos_sensor_byte(0x5400,0x20);
	write_cmos_sensor_byte(0x5401,0xff);
	write_cmos_sensor_byte(0x5402,0x0f);
	write_cmos_sensor_byte(0x5403,0x20);
	write_cmos_sensor_byte(0x5404,0x00);
	write_cmos_sensor_byte(0x5405,0x78);
	write_cmos_sensor_byte(0x5406,0x0f);
	write_cmos_sensor_byte(0x5407,0x97);
	write_cmos_sensor_byte(0x5408,0x00);
	write_cmos_sensor_byte(0x5409,0xe0);
	write_cmos_sensor_byte(0x540a,0x05);
	write_cmos_sensor_byte(0x540b,0x27);
	write_cmos_sensor_byte(0x540c,0x11);
	write_cmos_sensor_byte(0x540d,0x11);
	write_cmos_sensor_byte(0x540e,0x01);
	write_cmos_sensor_byte(0x540f,0xbc);
	write_cmos_sensor_byte(0x5414,0x04);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x5500,0x20);
	write_cmos_sensor_byte(0x5501,0xff);
	write_cmos_sensor_byte(0x5502,0x0f);
	write_cmos_sensor_byte(0x5503,0x20);
	write_cmos_sensor_byte(0x5504,0x00);
	write_cmos_sensor_byte(0x5505,0x78);
	write_cmos_sensor_byte(0x5506,0x0f);
	write_cmos_sensor_byte(0x5507,0x97);
	write_cmos_sensor_byte(0x5508,0x00);
	write_cmos_sensor_byte(0x5509,0xe0);
	write_cmos_sensor_byte(0x550a,0x05);
	write_cmos_sensor_byte(0x550b,0x27);
	write_cmos_sensor_byte(0x550c,0x11);
	write_cmos_sensor_byte(0x550d,0x11);
	write_cmos_sensor_byte(0x550e,0x01);
	write_cmos_sensor_byte(0x550f,0x78);
	write_cmos_sensor_byte(0x5514,0x04);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x00);
	write_cmos_sensor_byte(0x5809,0x49);
	write_cmos_sensor_byte(0x580a,0x0c);
	write_cmos_sensor_byte(0x580b,0x18);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x00);
	write_cmos_sensor_byte(0x5827,0x00);
	write_cmos_sensor_byte(0x5828,0x0c);
	write_cmos_sensor_byte(0x5829,0x5f);
	write_cmos_sensor_byte(0x5900,0x01);
	write_cmos_sensor_byte(0x5901,0x84);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
}

static void custom1_setting(void)
{
	LOG_INF("costom1_setting \n");
	write_cmos_sensor_byte(0x0103,0x01);
	write_cmos_sensor_byte(0x0100,0x00);
	write_cmos_sensor_byte(0x301f,0x10);
	write_cmos_sensor_byte(0x3033,0x21);
	write_cmos_sensor_byte(0x3058,0x10);
	write_cmos_sensor_byte(0x3059,0x86);
	write_cmos_sensor_byte(0x305a,0x43);
	write_cmos_sensor_byte(0x305b,0x52);
	write_cmos_sensor_byte(0x305c,0x09);
	write_cmos_sensor_byte(0x305e,0x00);
	write_cmos_sensor_byte(0x3208,0x10);
	write_cmos_sensor_byte(0x3209,0x00);
	write_cmos_sensor_byte(0x320a,0x0c);
	write_cmos_sensor_byte(0x320b,0x00);
	write_cmos_sensor_byte(0x320c,0x04);
	write_cmos_sensor_byte(0x320d,0xd8);
	write_cmos_sensor_byte(0x320e,0x0c);
	write_cmos_sensor_byte(0x320f,0x98);
	write_cmos_sensor_byte(0x3211,0x08);
	write_cmos_sensor_byte(0x3213,0x30);
	write_cmos_sensor_byte(0x3214,0x22);
	write_cmos_sensor_byte(0x3215,0x22);
	write_cmos_sensor_byte(0x321a,0x10);
	write_cmos_sensor_byte(0x3220,0x02);
	write_cmos_sensor_byte(0x3225,0x21);
	write_cmos_sensor_byte(0x3226,0x05);
	write_cmos_sensor_byte(0x3227,0x00);
	write_cmos_sensor_byte(0x3250,0x80);
	write_cmos_sensor_byte(0x3253,0x20);
	write_cmos_sensor_byte(0x325f,0x40);
	write_cmos_sensor_byte(0x3280,0x0d);
	write_cmos_sensor_byte(0x3284,0x02);
	write_cmos_sensor_byte(0x3287,0x20);
	write_cmos_sensor_byte(0x329d,0x8c);
	write_cmos_sensor_byte(0x32d1,0x30);
	write_cmos_sensor_byte(0x3301,0x0e);
	write_cmos_sensor_byte(0x3302,0x0c);
	write_cmos_sensor_byte(0x3304,0x48);
	write_cmos_sensor_byte(0x3306,0x58);
	write_cmos_sensor_byte(0x3308,0x12);
	write_cmos_sensor_byte(0x3309,0x60);
	write_cmos_sensor_byte(0x330b,0xb8);
	write_cmos_sensor_byte(0x330d,0x10);
	write_cmos_sensor_byte(0x330e,0x3e);
	write_cmos_sensor_byte(0x330f,0x02);
	write_cmos_sensor_byte(0x3310,0x02);
	write_cmos_sensor_byte(0x3314,0x13);
	write_cmos_sensor_byte(0x331e,0x39);
	write_cmos_sensor_byte(0x331f,0x51);
	write_cmos_sensor_byte(0x3333,0x10);
	write_cmos_sensor_byte(0x3334,0x40);
	write_cmos_sensor_byte(0x3347,0x05);
	write_cmos_sensor_byte(0x334c,0x10);
	write_cmos_sensor_byte(0x335d,0x60);
	write_cmos_sensor_byte(0x3399,0x0a);
	write_cmos_sensor_byte(0x33ad,0x20);
	write_cmos_sensor_byte(0x33b2,0x60);
	write_cmos_sensor_byte(0x33b3,0x18);
	write_cmos_sensor_byte(0x342a,0xff);
	write_cmos_sensor_byte(0x34ba,0x08);
	write_cmos_sensor_byte(0x34bb,0x01);
	write_cmos_sensor_byte(0x34f5,0x0c);
	write_cmos_sensor_byte(0x3616,0xbc);
	write_cmos_sensor_byte(0x3633,0x46);
	write_cmos_sensor_byte(0x3635,0x20);
	write_cmos_sensor_byte(0x3636,0xc5);
	write_cmos_sensor_byte(0x3637,0x3a);
	write_cmos_sensor_byte(0x3638,0xff);
	write_cmos_sensor_byte(0x363b,0x04);
	write_cmos_sensor_byte(0x363c,0x0e);
	write_cmos_sensor_byte(0x363f,0x10);
	write_cmos_sensor_byte(0x3649,0x60);
	write_cmos_sensor_byte(0x3670,0xa2);
	write_cmos_sensor_byte(0x3671,0x41);
	write_cmos_sensor_byte(0x3672,0x21);
	write_cmos_sensor_byte(0x3673,0x80);
	write_cmos_sensor_byte(0x3690,0x34);
	write_cmos_sensor_byte(0x3691,0x46);
	write_cmos_sensor_byte(0x3692,0x46);
	write_cmos_sensor_byte(0x369c,0x80);
	write_cmos_sensor_byte(0x369d,0x98);
	write_cmos_sensor_byte(0x36b5,0x80);
	write_cmos_sensor_byte(0x36b6,0xb8);
	write_cmos_sensor_byte(0x36b7,0xb8);
	write_cmos_sensor_byte(0x36bf,0x0b);
	write_cmos_sensor_byte(0x36c0,0x18);
	write_cmos_sensor_byte(0x36c1,0x18);
	write_cmos_sensor_byte(0x36c2,0x54);
	write_cmos_sensor_byte(0x36d4,0x88);
	write_cmos_sensor_byte(0x36d5,0xb8);
	write_cmos_sensor_byte(0x36e9,0x00);
	write_cmos_sensor_byte(0x36ea,0x15);
	write_cmos_sensor_byte(0x370f,0x05);
	write_cmos_sensor_byte(0x3714,0x00);
	write_cmos_sensor_byte(0x3724,0xb1);
	write_cmos_sensor_byte(0x3728,0x88);
	write_cmos_sensor_byte(0x372b,0xd3);
	write_cmos_sensor_byte(0x3771,0x1f);
	write_cmos_sensor_byte(0x3772,0x1f);
	write_cmos_sensor_byte(0x3773,0x1b);
	write_cmos_sensor_byte(0x377a,0x88);
	write_cmos_sensor_byte(0x377b,0xb8);
	write_cmos_sensor_byte(0x3818,0x01);
	write_cmos_sensor_byte(0x3900,0x0f);
	write_cmos_sensor_byte(0x3901,0x10);
	write_cmos_sensor_byte(0x3902,0xe0);
	write_cmos_sensor_byte(0x3903,0x02);
	write_cmos_sensor_byte(0x3904,0x38);
	write_cmos_sensor_byte(0x3908,0x40);
	write_cmos_sensor_byte(0x391a,0x2a);
	write_cmos_sensor_byte(0x391b,0x20);
	write_cmos_sensor_byte(0x391c,0x10);
	write_cmos_sensor_byte(0x391d,0x00);
	write_cmos_sensor_byte(0x391e,0x09);
	write_cmos_sensor_byte(0x391f,0xc1);
	write_cmos_sensor_byte(0x3926,0xe0);
	write_cmos_sensor_byte(0x3929,0x18);
	write_cmos_sensor_byte(0x39c8,0x00);
	write_cmos_sensor_byte(0x39c9,0x90);
	write_cmos_sensor_byte(0x39dd,0x04);
	write_cmos_sensor_byte(0x39de,0x04);
	write_cmos_sensor_byte(0x39e7,0x08);
	write_cmos_sensor_byte(0x39e8,0x40);
	write_cmos_sensor_byte(0x39e9,0x80);
	write_cmos_sensor_byte(0x3b09,0x00);
	write_cmos_sensor_byte(0x3b0a,0x00);
	write_cmos_sensor_byte(0x3b0b,0x00);
	write_cmos_sensor_byte(0x3b0c,0x00);
	write_cmos_sensor_byte(0x3b0d,0x10);
	write_cmos_sensor_byte(0x3b0e,0x10);
	write_cmos_sensor_byte(0x3b0f,0x10);
	write_cmos_sensor_byte(0x3b10,0x10);
	write_cmos_sensor_byte(0x3b11,0x20);
	write_cmos_sensor_byte(0x3b12,0x20);
	write_cmos_sensor_byte(0x3b13,0x20);
	write_cmos_sensor_byte(0x3b14,0x20);
	write_cmos_sensor_byte(0x3b15,0x30);
	write_cmos_sensor_byte(0x3b16,0x30);
	write_cmos_sensor_byte(0x3b17,0x30);
	write_cmos_sensor_byte(0x3b18,0x30);
	write_cmos_sensor_byte(0x3b49,0x03);
	write_cmos_sensor_byte(0x3b4c,0x03);
	write_cmos_sensor_byte(0x3b4d,0xff);
	write_cmos_sensor_byte(0x3b50,0x03);
	write_cmos_sensor_byte(0x3b51,0x7a);
	write_cmos_sensor_byte(0x3b52,0x03);
	write_cmos_sensor_byte(0x3b53,0x7b);
	write_cmos_sensor_byte(0x3b54,0x03);
	write_cmos_sensor_byte(0x3b55,0x80);
	write_cmos_sensor_byte(0x3b5a,0x03);
	write_cmos_sensor_byte(0x3b5b,0xf8);
	write_cmos_sensor_byte(0x3b5c,0x03);
	write_cmos_sensor_byte(0x3b5d,0xf9);
	write_cmos_sensor_byte(0x3b64,0x03);
	write_cmos_sensor_byte(0x3b65,0x7c);
	write_cmos_sensor_byte(0x3b66,0x03);
	write_cmos_sensor_byte(0x3b67,0x7b);
	write_cmos_sensor_byte(0x3b68,0x03);
	write_cmos_sensor_byte(0x3b69,0x80);
	write_cmos_sensor_byte(0x3b6e,0x03);
	write_cmos_sensor_byte(0x3b6f,0xf8);
	write_cmos_sensor_byte(0x3b70,0x03);
	write_cmos_sensor_byte(0x3b71,0xfc);
	write_cmos_sensor_byte(0x3c00,0x08);
	write_cmos_sensor_byte(0x3c06,0x07);
	write_cmos_sensor_byte(0x3c07,0x02);
	write_cmos_sensor_byte(0x3c2c,0x47);
	write_cmos_sensor_byte(0x3c65,0x05);
	write_cmos_sensor_byte(0x3c86,0x30);
	write_cmos_sensor_byte(0x3c87,0x01);
	write_cmos_sensor_byte(0x3cc6,0x30);
	write_cmos_sensor_byte(0x3cc7,0x01);
	write_cmos_sensor_byte(0x3e00,0x00);
	write_cmos_sensor_byte(0x3e01,0x20);
	write_cmos_sensor_byte(0x3e03,0x03);
	write_cmos_sensor_byte(0x3e06,0x01);
	write_cmos_sensor_byte(0x3e08,0x00);
	write_cmos_sensor_byte(0x3e09,0x80);
	write_cmos_sensor_byte(0x3e0c,0x01);
	write_cmos_sensor_byte(0x3e14,0xb1);
	write_cmos_sensor_byte(0x3e16,0x02);
	write_cmos_sensor_byte(0x3e17,0x00);
	write_cmos_sensor_byte(0x3e18,0x02);
	write_cmos_sensor_byte(0x3e19,0x00);
	write_cmos_sensor_byte(0x3e25,0xff);
	write_cmos_sensor_byte(0x3e26,0xff);
	write_cmos_sensor_byte(0x3e29,0x08);
	write_cmos_sensor_byte(0x4200,0x88);
	write_cmos_sensor_byte(0x4209,0x08);
	write_cmos_sensor_byte(0x420b,0x18);
	write_cmos_sensor_byte(0x420d,0x38);
	write_cmos_sensor_byte(0x420f,0x78);
	write_cmos_sensor_byte(0x4210,0x12);
	write_cmos_sensor_byte(0x4211,0x10);
	write_cmos_sensor_byte(0x4212,0x10);
	write_cmos_sensor_byte(0x4213,0x08);
	write_cmos_sensor_byte(0x4215,0x88);
	write_cmos_sensor_byte(0x4217,0x98);
	write_cmos_sensor_byte(0x4219,0xb8);
	write_cmos_sensor_byte(0x421b,0xf8);
	write_cmos_sensor_byte(0x421c,0x10);
	write_cmos_sensor_byte(0x421d,0x10);
	write_cmos_sensor_byte(0x421e,0x12);
	write_cmos_sensor_byte(0x421f,0x12);
	write_cmos_sensor_byte(0x42e8,0x0c);
	write_cmos_sensor_byte(0x42e9,0x0c);
	write_cmos_sensor_byte(0x42ea,0x0c);
	write_cmos_sensor_byte(0x42eb,0x0c);
	write_cmos_sensor_byte(0x42ec,0x0c);
	write_cmos_sensor_byte(0x42ed,0x0c);
	write_cmos_sensor_byte(0x42ee,0x0c);
	write_cmos_sensor_byte(0x42ef,0x0c);
	write_cmos_sensor_byte(0x43ac,0x80);
	write_cmos_sensor_byte(0x4402,0x03);
	write_cmos_sensor_byte(0x4403,0x0c);
	write_cmos_sensor_byte(0x4404,0x24);
	write_cmos_sensor_byte(0x4405,0x2f);
	write_cmos_sensor_byte(0x4406,0x01);
	write_cmos_sensor_byte(0x4407,0x20);
	write_cmos_sensor_byte(0x440c,0x3c);
	write_cmos_sensor_byte(0x440d,0x3c);
	write_cmos_sensor_byte(0x440e,0x2d);
	write_cmos_sensor_byte(0x440f,0x4b);
	write_cmos_sensor_byte(0x4411,0x01);
	write_cmos_sensor_byte(0x4412,0x01);
	write_cmos_sensor_byte(0x4413,0x00);
	write_cmos_sensor_byte(0x4424,0x01);
	write_cmos_sensor_byte(0x4502,0x24);
	write_cmos_sensor_byte(0x4503,0x2c);
	write_cmos_sensor_byte(0x4506,0xa2);
	write_cmos_sensor_byte(0x4509,0x18);
	write_cmos_sensor_byte(0x450d,0x08);
	write_cmos_sensor_byte(0x4512,0x01);
	write_cmos_sensor_byte(0x4516,0x01);
	write_cmos_sensor_byte(0x4800,0x24);
	write_cmos_sensor_byte(0x4837,0x16);
	write_cmos_sensor_byte(0x5000,0x0f);
	write_cmos_sensor_byte(0x5002,0xd8);
	write_cmos_sensor_byte(0x5003,0xf0);
	write_cmos_sensor_byte(0x5004,0x00);
	write_cmos_sensor_byte(0x5009,0x0f);
	write_cmos_sensor_byte(0x5016,0x5a);
	write_cmos_sensor_byte(0x5017,0x5a);
	write_cmos_sensor_byte(0x5018,0x00);
	write_cmos_sensor_byte(0x5019,0x21);
	write_cmos_sensor_byte(0x501a,0x0f);
	write_cmos_sensor_byte(0x501b,0xf0);
	write_cmos_sensor_byte(0x501c,0x00);
	write_cmos_sensor_byte(0x501d,0x21);
	write_cmos_sensor_byte(0x501e,0x0b);
	write_cmos_sensor_byte(0x501f,0xf0);
	write_cmos_sensor_byte(0x5020,0x40);
	write_cmos_sensor_byte(0x5075,0xcd);
	write_cmos_sensor_byte(0x507f,0xe2);
	write_cmos_sensor_byte(0x5184,0x10);
	write_cmos_sensor_byte(0x5185,0x08);
	write_cmos_sensor_byte(0x5187,0x1f);
	write_cmos_sensor_byte(0x5188,0x1f);
	write_cmos_sensor_byte(0x5189,0x1f);
	write_cmos_sensor_byte(0x518a,0x1f);
	write_cmos_sensor_byte(0x518b,0x1f);
	write_cmos_sensor_byte(0x518c,0x1f);
	write_cmos_sensor_byte(0x518d,0x40);
	write_cmos_sensor_byte(0x5190,0x20);
	write_cmos_sensor_byte(0x5191,0x20);
	write_cmos_sensor_byte(0x5192,0x20);
	write_cmos_sensor_byte(0x5193,0x20);
	write_cmos_sensor_byte(0x5194,0x20);
	write_cmos_sensor_byte(0x5195,0x20);
	write_cmos_sensor_byte(0x5199,0x44);
	write_cmos_sensor_byte(0x519a,0x77);
	write_cmos_sensor_byte(0x51aa,0x2a);
	write_cmos_sensor_byte(0x51ab,0x7f);
	write_cmos_sensor_byte(0x51ac,0x00);
	write_cmos_sensor_byte(0x51ad,0x00);
	write_cmos_sensor_byte(0x51be,0x01);
	write_cmos_sensor_byte(0x51c4,0x10);
	write_cmos_sensor_byte(0x51c5,0x08);
	write_cmos_sensor_byte(0x51c7,0x1f);
	write_cmos_sensor_byte(0x51c8,0x1f);
	write_cmos_sensor_byte(0x51c9,0x1f);
	write_cmos_sensor_byte(0x51ca,0x1f);
	write_cmos_sensor_byte(0x51cb,0x1f);
	write_cmos_sensor_byte(0x51cc,0x1f);
	write_cmos_sensor_byte(0x51d0,0x20);
	write_cmos_sensor_byte(0x51d1,0x20);
	write_cmos_sensor_byte(0x51d2,0x20);
	write_cmos_sensor_byte(0x51d3,0x20);
	write_cmos_sensor_byte(0x51d4,0x20);
	write_cmos_sensor_byte(0x51d5,0x20);
	write_cmos_sensor_byte(0x51d9,0x44);
	write_cmos_sensor_byte(0x51da,0x77);
	write_cmos_sensor_byte(0x51eb,0x7f);
	write_cmos_sensor_byte(0x51ec,0x00);
	write_cmos_sensor_byte(0x51ed,0x00);
	write_cmos_sensor_byte(0x53c0,0x0c);
	write_cmos_sensor_byte(0x53c5,0x40);
	write_cmos_sensor_byte(0x5401,0x08);
	write_cmos_sensor_byte(0x540a,0x0c);
	write_cmos_sensor_byte(0x540b,0x5f);
	write_cmos_sensor_byte(0x540e,0x03);
	write_cmos_sensor_byte(0x540f,0x70);
	write_cmos_sensor_byte(0x5414,0x0c);
	write_cmos_sensor_byte(0x5415,0x80);
	write_cmos_sensor_byte(0x5416,0x64);
	write_cmos_sensor_byte(0x541b,0x00);
	write_cmos_sensor_byte(0x541c,0x49);
	write_cmos_sensor_byte(0x541d,0x0c);
	write_cmos_sensor_byte(0x541e,0x18);
	write_cmos_sensor_byte(0x5501,0x08);
	write_cmos_sensor_byte(0x550a,0x0c);
	write_cmos_sensor_byte(0x550b,0x5f);
	write_cmos_sensor_byte(0x550e,0x02);
	write_cmos_sensor_byte(0x550f,0x00);
	write_cmos_sensor_byte(0x5514,0x0c);
	write_cmos_sensor_byte(0x5515,0x80);
	write_cmos_sensor_byte(0x5516,0x64);
	write_cmos_sensor_byte(0x551b,0x00);
	write_cmos_sensor_byte(0x551c,0x49);
	write_cmos_sensor_byte(0x551d,0x0c);
	write_cmos_sensor_byte(0x551e,0x18);
	write_cmos_sensor_byte(0x5800,0x30);
	write_cmos_sensor_byte(0x5801,0x4a);
	write_cmos_sensor_byte(0x5802,0x06);
	write_cmos_sensor_byte(0x5803,0x01);
	write_cmos_sensor_byte(0x5804,0x00);
	write_cmos_sensor_byte(0x5805,0x21);
	write_cmos_sensor_byte(0x5806,0x0f);
	write_cmos_sensor_byte(0x5807,0xf0);
	write_cmos_sensor_byte(0x5808,0x00);
	write_cmos_sensor_byte(0x5809,0x49);
	write_cmos_sensor_byte(0x580a,0x0c);
	write_cmos_sensor_byte(0x580b,0x18);
	write_cmos_sensor_byte(0x580c,0xff);
	write_cmos_sensor_byte(0x580d,0x32);
	write_cmos_sensor_byte(0x580e,0x03);
	write_cmos_sensor_byte(0x580f,0x14);
	write_cmos_sensor_byte(0x5810,0x04);
	write_cmos_sensor_byte(0x5811,0x00);
	write_cmos_sensor_byte(0x5812,0x00);
	write_cmos_sensor_byte(0x5813,0x00);
	write_cmos_sensor_byte(0x5814,0x80);
	write_cmos_sensor_byte(0x5815,0x80);
	write_cmos_sensor_byte(0x5816,0x80);
	write_cmos_sensor_byte(0x5817,0xff);
	write_cmos_sensor_byte(0x5818,0xff);
	write_cmos_sensor_byte(0x5819,0xff);
	write_cmos_sensor_byte(0x581a,0x00);
	write_cmos_sensor_byte(0x581b,0x00);
	write_cmos_sensor_byte(0x581c,0x02);
	write_cmos_sensor_byte(0x581d,0x00);
	write_cmos_sensor_byte(0x581e,0x5a);
	write_cmos_sensor_byte(0x581f,0x5a);
	write_cmos_sensor_byte(0x5820,0x08);
	write_cmos_sensor_byte(0x5821,0x01);
	write_cmos_sensor_byte(0x5822,0x00);
	write_cmos_sensor_byte(0x5823,0x00);
	write_cmos_sensor_byte(0x5824,0x10);
	write_cmos_sensor_byte(0x5825,0x0f);
	write_cmos_sensor_byte(0x5826,0x00);
	write_cmos_sensor_byte(0x5827,0x00);
	write_cmos_sensor_byte(0x5828,0x0c);
	write_cmos_sensor_byte(0x5829,0x5f);
	write_cmos_sensor_byte(0x5f00,0x05);
	write_cmos_sensor_byte(0x5f06,0x00);
	write_cmos_sensor_byte(0x5f07,0x42);
	write_cmos_sensor_byte(0x5f08,0x1f);
	write_cmos_sensor_byte(0x5f09,0xe1);
	write_cmos_sensor_byte(0x5f0c,0x00);
	write_cmos_sensor_byte(0x5f0d,0x92);
	write_cmos_sensor_byte(0x5f0e,0x18);
	write_cmos_sensor_byte(0x5f0f,0x31);
	write_cmos_sensor_byte(0x5f16,0x00);
	write_cmos_sensor_byte(0x5f17,0x00);
	write_cmos_sensor_byte(0x5f18,0x20);
	write_cmos_sensor_byte(0x5f19,0x1f);
	write_cmos_sensor_byte(0x5f1a,0x00);
	write_cmos_sensor_byte(0x5f1b,0x00);
	write_cmos_sensor_byte(0x5f1c,0x18);
	write_cmos_sensor_byte(0x5f1d,0xbf);
	write_cmos_sensor_byte(0x5f1e,0x10);
	write_cmos_sensor_byte(0x611a,0x01);
	write_cmos_sensor_byte(0x611c,0x01);
	write_cmos_sensor_byte(0x611e,0x01);
	write_cmos_sensor_byte(0x6120,0x01);
	write_cmos_sensor_byte(0x6122,0x01);
	write_cmos_sensor_byte(0x6124,0x01);
	write_cmos_sensor_byte(0x6126,0x01);
	write_cmos_sensor_byte(0x6128,0x01);
	write_cmos_sensor_byte(0x612a,0x01);
	write_cmos_sensor_byte(0x612c,0x01);
	write_cmos_sensor_byte(0x612e,0x01);
	write_cmos_sensor_byte(0x6130,0x01);
	write_cmos_sensor_byte(0x6176,0x01);
	write_cmos_sensor_byte(0x6178,0x01);
	write_cmos_sensor_byte(0x617a,0x01);
	write_cmos_sensor_byte(0x6186,0x01);
	write_cmos_sensor_byte(0x6188,0x01);
	write_cmos_sensor_byte(0x61b9,0xe0);
	write_cmos_sensor_byte(0x61bd,0x02);
	write_cmos_sensor_byte(0x61bf,0x00);
	write_cmos_sensor_byte(0x61e1,0x03);
	write_cmos_sensor_byte(0x61e2,0xf0);
	write_cmos_sensor_byte(0x61e3,0x02);
	write_cmos_sensor_byte(0x61e4,0xf4);
	write_cmos_sensor_byte(0x61e5,0x01);
	write_cmos_sensor_byte(0x6c11,0x04);
	write_cmos_sensor_byte(0x6e00,0x03);
	write_cmos_sensor_byte(0x6e05,0x66);
	write_cmos_sensor_byte(0x6e06,0x66);
	write_cmos_sensor_byte(0x6e07,0x66);
	write_cmos_sensor_byte(0x6e08,0x66);
	write_cmos_sensor_byte(0x6e09,0xff);
	write_cmos_sensor_byte(0x6e0a,0x19);
	write_cmos_sensor_byte(0x6e0b,0x19);
	write_cmos_sensor_byte(0x6e0c,0x19);
	write_cmos_sensor_byte(0x6e0d,0x19);
	write_cmos_sensor_byte(0x6e0e,0x19);
	write_cmos_sensor_byte(0x6e0f,0x19);
	write_cmos_sensor_byte(0x6e10,0x19);
	write_cmos_sensor_byte(0x6e11,0x19);
	write_cmos_sensor_byte(0x6e12,0xff);
	write_cmos_sensor_byte(0x6e13,0x19);
	write_cmos_sensor_byte(0x6e14,0x19);
	write_cmos_sensor_byte(0x6e15,0x19);
	write_cmos_sensor_byte(0x6e16,0x19);
	write_cmos_sensor_byte(0x6e17,0x19);
	write_cmos_sensor_byte(0x6e18,0x19);
	write_cmos_sensor_byte(0x6e19,0x19);
	write_cmos_sensor_byte(0x6e1a,0x19);
	write_cmos_sensor_byte(0x6e1b,0xff);
	write_cmos_sensor_byte(0x6e1c,0x19);
	write_cmos_sensor_byte(0x6e1d,0x19);
	write_cmos_sensor_byte(0x6e1e,0x19);
	write_cmos_sensor_byte(0x6e1f,0x19);
	write_cmos_sensor_byte(0x6e20,0x19);
	write_cmos_sensor_byte(0x6e21,0x19);
	write_cmos_sensor_byte(0x6e22,0x19);
	write_cmos_sensor_byte(0x6e23,0x19);
	write_cmos_sensor_byte(0x6e24,0xff);
	write_cmos_sensor_byte(0x6e25,0x19);
	write_cmos_sensor_byte(0x6e26,0x19);
	write_cmos_sensor_byte(0x6e27,0x19);
	write_cmos_sensor_byte(0x6e28,0x19);
	write_cmos_sensor_byte(0x6e29,0x19);
	write_cmos_sensor_byte(0x6e2a,0x19);
	write_cmos_sensor_byte(0x6e2b,0x19);
	write_cmos_sensor_byte(0x6e2c,0x19);
	write_cmos_sensor_byte(0x6e2d,0xff);
	write_cmos_sensor_byte(0x6e2e,0x19);
	write_cmos_sensor_byte(0x6e2f,0x19);
	write_cmos_sensor_byte(0x6e30,0x19);
	write_cmos_sensor_byte(0x6e31,0x19);
	write_cmos_sensor_byte(0x6e32,0x19);
	write_cmos_sensor_byte(0x6e33,0x19);
	write_cmos_sensor_byte(0x6e34,0x19);
	write_cmos_sensor_byte(0x6e35,0x19);
	write_cmos_sensor_byte(0x6e36,0xff);
	write_cmos_sensor_byte(0x6e37,0x19);
	write_cmos_sensor_byte(0x6e38,0x19);
	write_cmos_sensor_byte(0x6e39,0x19);
	write_cmos_sensor_byte(0x6e3a,0x19);
	write_cmos_sensor_byte(0x6e3b,0x19);
	write_cmos_sensor_byte(0x6e3c,0x19);
	write_cmos_sensor_byte(0x6e3d,0x19);
	write_cmos_sensor_byte(0x6e3e,0x19);
	write_cmos_sensor_byte(0x6e3f,0xff);
	write_cmos_sensor_byte(0x6e40,0x19);
	write_cmos_sensor_byte(0x6e41,0x19);
	write_cmos_sensor_byte(0x6e42,0x19);
	write_cmos_sensor_byte(0x6e43,0x19);
	write_cmos_sensor_byte(0x6e44,0x19);
	write_cmos_sensor_byte(0x6e45,0x19);
	write_cmos_sensor_byte(0x6e46,0x19);
	write_cmos_sensor_byte(0x6e47,0x19);
	write_cmos_sensor_byte(0x6e48,0xff);
	write_cmos_sensor_byte(0x6e49,0x19);
	write_cmos_sensor_byte(0x6e4a,0x19);
	write_cmos_sensor_byte(0x6e4b,0x19);
	write_cmos_sensor_byte(0x6e4c,0x19);
	write_cmos_sensor_byte(0x6e4d,0x19);
	write_cmos_sensor_byte(0x6e4e,0x19);
	write_cmos_sensor_byte(0x6e4f,0x19);
	write_cmos_sensor_byte(0x6e50,0x19);
	write_cmos_sensor_byte(0x6e51,0xff);
	write_cmos_sensor_byte(0x6e52,0x19);
	write_cmos_sensor_byte(0x6e53,0x19);
	write_cmos_sensor_byte(0x6e54,0x19);
	write_cmos_sensor_byte(0x6e55,0x19);
	write_cmos_sensor_byte(0x6e56,0x19);
	write_cmos_sensor_byte(0x6e57,0x19);
	write_cmos_sensor_byte(0x6e58,0x19);
	write_cmos_sensor_byte(0x6e59,0x19);
	write_cmos_sensor_byte(0x6e5a,0xf8);
	write_cmos_sensor_byte(0x6e5b,0x19);
	write_cmos_sensor_byte(0x6e5c,0x19);
	write_cmos_sensor_byte(0x6e5d,0x19);
	write_cmos_sensor_byte(0x6e5e,0x19);
	write_cmos_sensor_byte(0x6e5f,0x19);
	write_cmos_sensor_byte(0x6e60,0xff);
	write_cmos_sensor_byte(0x6e61,0x19);
	write_cmos_sensor_byte(0x6e62,0x19);
	write_cmos_sensor_byte(0x6e63,0x19);
	write_cmos_sensor_byte(0x6e64,0x19);
	write_cmos_sensor_byte(0x6e65,0x19);
	write_cmos_sensor_byte(0x6e66,0x19);
	write_cmos_sensor_byte(0x6e67,0x19);
	write_cmos_sensor_byte(0x6e68,0x19);
	write_cmos_sensor_byte(0x6e69,0xff);
	write_cmos_sensor_byte(0x6e6a,0x19);
	write_cmos_sensor_byte(0x6e6b,0x19);
	write_cmos_sensor_byte(0x6e6c,0x19);
	write_cmos_sensor_byte(0x6e6d,0x19);
	write_cmos_sensor_byte(0x6e6e,0x19);
	write_cmos_sensor_byte(0x6e6f,0x19);
	write_cmos_sensor_byte(0x6e70,0x19);
	write_cmos_sensor_byte(0x6e71,0x19);
	write_cmos_sensor_byte(0x6e72,0xff);
	write_cmos_sensor_byte(0x6e73,0x19);
	write_cmos_sensor_byte(0x6e74,0x19);
	write_cmos_sensor_byte(0x6e75,0x19);
	write_cmos_sensor_byte(0x6e76,0x19);
	write_cmos_sensor_byte(0x6e77,0x19);
	write_cmos_sensor_byte(0x6e78,0x19);
	write_cmos_sensor_byte(0x6e79,0x19);
	write_cmos_sensor_byte(0x6e7a,0x19);
	write_cmos_sensor_byte(0x6e7b,0xff);
	write_cmos_sensor_byte(0x6e7c,0x19);
	write_cmos_sensor_byte(0x6e7d,0x19);
	write_cmos_sensor_byte(0x6e7e,0x19);
	write_cmos_sensor_byte(0x6e7f,0x19);
	write_cmos_sensor_byte(0x6e80,0x19);
	write_cmos_sensor_byte(0x6e81,0x19);
	write_cmos_sensor_byte(0x6e82,0x19);
	write_cmos_sensor_byte(0x6e83,0x19);
	write_cmos_sensor_byte(0x6e84,0xff);
	write_cmos_sensor_byte(0x6e85,0x19);
	write_cmos_sensor_byte(0x6e86,0x19);
	write_cmos_sensor_byte(0x6e87,0x19);
	write_cmos_sensor_byte(0x6e88,0x19);
	write_cmos_sensor_byte(0x6e89,0x19);
	write_cmos_sensor_byte(0x6e8a,0x19);
	write_cmos_sensor_byte(0x6e8b,0x19);
	write_cmos_sensor_byte(0x6e8c,0x19);
	write_cmos_sensor_byte(0x6e8d,0xff);
	write_cmos_sensor_byte(0x6e8e,0x19);
	write_cmos_sensor_byte(0x6e8f,0x19);
	write_cmos_sensor_byte(0x6e90,0x19);
	write_cmos_sensor_byte(0x6e91,0x19);
	write_cmos_sensor_byte(0x6e92,0x19);
	write_cmos_sensor_byte(0x6e93,0x19);
	write_cmos_sensor_byte(0x6e94,0x19);
	write_cmos_sensor_byte(0x6e95,0x19);
	write_cmos_sensor_byte(0x6e96,0xff);
	write_cmos_sensor_byte(0x6e97,0x19);
	write_cmos_sensor_byte(0x6e98,0x19);
	write_cmos_sensor_byte(0x6e99,0x19);
	write_cmos_sensor_byte(0x6e9a,0x19);
	write_cmos_sensor_byte(0x6e9b,0x19);
	write_cmos_sensor_byte(0x6e9c,0x19);
	write_cmos_sensor_byte(0x6e9d,0x19);
	write_cmos_sensor_byte(0x6e9e,0x19);
	write_cmos_sensor_byte(0x6e9f,0xff);
	write_cmos_sensor_byte(0x6ea0,0x19);
	write_cmos_sensor_byte(0x6ea1,0x19);
	write_cmos_sensor_byte(0x6ea2,0x19);
	write_cmos_sensor_byte(0x6ea3,0x19);
	write_cmos_sensor_byte(0x6ea4,0x19);
	write_cmos_sensor_byte(0x6ea5,0x19);
	write_cmos_sensor_byte(0x6ea6,0x19);
	write_cmos_sensor_byte(0x6ea7,0x19);
	write_cmos_sensor_byte(0x6ea8,0xff);
	write_cmos_sensor_byte(0x6ea9,0x19);
	write_cmos_sensor_byte(0x6eaa,0x19);
	write_cmos_sensor_byte(0x6eab,0x19);
	write_cmos_sensor_byte(0x6eac,0x19);
	write_cmos_sensor_byte(0x6ead,0x19);
	write_cmos_sensor_byte(0x6eae,0x19);
	write_cmos_sensor_byte(0x6eaf,0x19);
	write_cmos_sensor_byte(0x6eb0,0x19);
	write_cmos_sensor_byte(0x6eb1,0xf8);
	write_cmos_sensor_byte(0x6eb2,0x19);
	write_cmos_sensor_byte(0x6eb3,0x19);
	write_cmos_sensor_byte(0x6eb4,0x19);
	write_cmos_sensor_byte(0x6eb5,0x19);
	write_cmos_sensor_byte(0x6eb6,0x19);
	write_cmos_sensor_byte(0x6ec1,0x00);
	write_cmos_sensor_byte(0x6ec2,0x21);
	write_cmos_sensor_byte(0x6ec3,0x0f);
	write_cmos_sensor_byte(0x6ec4,0xf0);
	write_cmos_sensor_byte(0x6ec5,0x00);
	write_cmos_sensor_byte(0x6ec6,0x21);
	write_cmos_sensor_byte(0x6ec7,0x0b);
	write_cmos_sensor_byte(0x6ec8,0xf0);
	write_cmos_sensor_byte(0x6ec9,0x03);
	write_cmos_sensor_byte(0x6eca,0xf4);
	write_cmos_sensor_byte(0x6ecb,0x02);
	write_cmos_sensor_byte(0x6ecc,0xf4);
	write_cmos_sensor_byte(0x6ece,0x10);
	write_cmos_sensor_byte(0x320e,0x0f);
	write_cmos_sensor_byte(0x320f,0xbe);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x3107) << 8) | read_cmos_sensor_8(0x3108));
}

//static kal_uint16 get_vendor_id(void)
//{
//kal_uint16 get_byte = 0;
//char pusendcmd[2] = { (char)(0x01 >> 8), (char)(0x01 & 0xFF) };

//iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, 0xA2);

//return get_byte;
//}

/*************************************************************************
*FUNCTION
*get_imgsensor_id
*
*DESCRIPTION
*This function get the sensor ID
*
*PARAMETERS
**sensorID : return the sensor ID
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
extern int hbb_flag;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
kal_uint8 retry = 2 ;/*,vendor_id = 0;*/

	//vendor_id = get_vendor_id();
	//LOG_INF("get_imgsensor_idvendor_id: 0x%x\n",vendor_id);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			//if (vendor_id == 0x01) {
				*sensor_id = return_sensor_id();
					LOG_INF("get_imgsensor_idsensor_id: 0x%x\n",*sensor_id);
				if (*sensor_id == imgsensor_info.sensor_id) {
					pr_info("sc5000cs_sunny i2c 0x%x, sid 0x%x\n",
						imgsensor.i2c_write_id, *sensor_id);
#if 1
						sc5000cs_fusion_id_read();
						sc5000cs_sn_read();
#endif
					return ERROR_NONE;

				} else {
					LOG_INF
					("check id fail i2c 0x%x, sid: 0x%x\n",
						imgsensor.i2c_write_id,
						*sensor_id);
						*sensor_id = 0xFFFFFFFF;
				}
					LOG_INF
					("i2c id: 0x%x, ReadOut sid: 0x%x, info.sensor_id:0x%x.\n",
					imgsensor.i2c_write_id, *sensor_id,
					imgsensor_info.sensor_id);
			//}
			LOG_INF
			("Read fail, i2cid: 0x%x, Rsid: 0x%x, info.sid:0x%x.\n",
			imgsensor.i2c_write_id, *sensor_id,
			imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {

		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*open
*
*DESCRIPTION
*This function initialize the registers of CMOS sensor
*
*PARAMETERS
*None
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{

	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			LOG_INF("open sensor_id: 0x%x, imgsensor_info.sensor_id \n", sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF
				("i2c write id : 0x%x, sensor id: 0x%x\n",
				 imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF
			("Read sensor id fail , id: 0x%x, sensor id: 0x%x\n",
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

	sensor_init();
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	//qvga_i2c4_mclk();

	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*close
*
*DESCRIPTION
*
*
*PARAMETERS
*None
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*preview
*
*DESCRIPTION
*This function start the sensor preview.
*
*PARAMETERS
**image_window : address pointer of pixel numbers in one period of HSYNC
**sensor_config_data : address pointer of line numbers in one period of VSYNC
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32
preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("preview start\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*capture
*
*DESCRIPTION
*This function setup the CMOS sensor in capture MY_OUTPUT mode
*
*PARAMETERS
*
*RETURNS
*None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32
capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("capture start\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		LOG_INF("capture30fps: use cap30FPS's setting: %d fps!\n",
				imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		LOG_INF("cap115fps: use cap1's setting: %d fps!\n",
				imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		LOG_INF
		("Warning:current_fps %d is not support, use cap1\n",
		 imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_HV_MIRROR);
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32
normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
	 image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video start\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

static kal_uint32
hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		 image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video start\n");
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
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

static kal_uint32
slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		 image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video start\n");
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
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

static kal_uint32
custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;

	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
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

	sensor_resolution->SensorSlimVideoWidth	 =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;
	return ERROR_NONE;
}

static kal_uint32
get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		 MSDK_SENSOR_INFO_STRUCT *sensor_info,
		 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;
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
#ifdef FPTPDAFSUPPORT
	sensor_info->PDAF_Support = 2;
#else
	sensor_info->PDAF_Support = 0;
#endif
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
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

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

static kal_uint32
control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
	//LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_FALSE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32
set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
	scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
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
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length -
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
			(frame_length > imgsensor_info.cap1.framelength)
			 ? (frame_length - imgsensor_info.cap1.framelength)
			: 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
					imgsensor_info.cap.max_framerate)
				LOG_INF
		("current_fps %d is not support, so use cap' %d fps!\n",
				 framerate,
				 imgsensor_info.cap.max_framerate / 10);
			frame_length =
				imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap.framelength)
				 ? (frame_length -
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
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			 + imgsensor.dummy_line;
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

static kal_uint32 get_default_framerate_by_scenario(enum
		MSDK_SCENARIO_ID_ENUM
		scenario_id,
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
		write_cmos_sensor_byte(0x0100, 0x00);
		write_cmos_sensor_byte(0x3902, 0xa0);
		write_cmos_sensor_byte(0x3909, 0xff);
		write_cmos_sensor_byte(0x390a, 0xff);
		write_cmos_sensor_byte(0x391f, 0x00);
		write_cmos_sensor_byte(0x0100, 0x01);
	} else {
		write_cmos_sensor_byte(0x0100, 0x00);
		write_cmos_sensor_byte(0x3902, 0xe0);
		write_cmos_sensor_byte(0x3909, 0x00);
		write_cmos_sensor_byte(0x390a, 0x00);
		write_cmos_sensor_byte(0x391f, 0x41);
		write_cmos_sensor_byte(0x0100, 0x01);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;

}

static kal_uint32
feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	//UINT32 fps = 0;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//pr_debug("feature_id = %d, len=%d\n", feature_id, *feature_para_len);
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
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

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
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		pr_debug("imgsensor.pclk = %d,current_fps = %d\n",
				 imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((kal_uint32)*feature_data);
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
		write_cmos_sensor(sensor_reg_data->RegAddr,
						sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM or
		 *just return LENS_DRIVER_ID_DO_NOT_CARE
		 */

		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) * feature_data_16,
			(UINT16) *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
			*feature_data, (UINT32) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
			*(feature_data), (MUINT32 *) (uintptr_t) (*
			(feature_data + 1)));
		break;

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", (UINT16) *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", (UINT16) *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				 (UINT32) *feature_data_32);

		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT
			 *)(uintptr_t) (*(feature_data + 1));

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
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)(*feature_data), (UINT16)(*(feature_data + 1)), (BOOL) (*(feature_data + 2)));
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
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				 (UINT16) *feature_data,
				 (UINT16) *(feature_data + 1));
		/*ihdr_write_shutter(
		 *(UINT16)*feature_data,(UINT16)*(feature_data+1));
		 */
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

	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
				 (UINT16) *feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T
			 *)(uintptr_t) (*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				 sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info_16_9,
				 sizeof(struct SET_PD_BLOCK_INFO_T));
			break;

		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
				 (UINT16) *feature_data);
		pvcinfo =
			(struct SENSOR_VC_INFO_STRUCT
			 *)(uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[1],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[2],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[3],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[4],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[5],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo,
				 (void *)&SENSOR_VC_INFO[0],
				 sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug
		("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		 (UINT16) *feature_data);

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", (UINT8) *feature_data_16);
		imgsensor.pdaf_mode = (UINT8) *feature_data_16;
		break;

	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%d\n",
				 (UINT16) *feature_data);
		if (*feature_data != 0)
			set_shutter((UINT32) *feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				 (imgsensor_info.cap.linelength - 80)) *
				imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				 (imgsensor_info.normal_video.linelength - 80))
				 *imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.hs_video.pclk /
				 (imgsensor_info.hs_video.linelength - 80)) *
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
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				 (imgsensor_info.pre.linelength - 80)) *
				imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		//fps = (MUINT32) (*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
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
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}

		break;

//cxc long exposure >
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
			*feature_return_para_32 =
				imgsensor.current_ae_effective_frame;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
			memcpy(feature_return_para_32, &imgsensor.ae_frm_mode,
				sizeof(struct IMGSENSOR_AE_FRM_MODE));
		break;
//cxc long exposure <

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


UINT32 SC5000_TRULY_MAIN_II_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	LOG_INF("SC5000_TRULY_MAIN_II_MIPI_RAW_SensorInit in\n");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =&sensor_func;
	return ERROR_NONE;
}	/*	SC5000CS_MIPI_RAW_SensorInit	*/
