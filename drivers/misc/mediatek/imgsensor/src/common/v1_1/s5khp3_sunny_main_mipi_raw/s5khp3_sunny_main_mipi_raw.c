/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5khp3_sunny_main_mipi_raw.c
 *
 * Project:
 * --------
 *	 ALPS MT6795
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * sensor setting : AM11a
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <asm/neon.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#ifdef CONFIG_RLK_CAM_PERFORMANCE_IMPROVE
#include <linux/dma-mapping.h>
#endif
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include <linux/slab.h>
#include "s5khp3_sunny_main_mipi_raw.h"

#define ENABLE_PDAF 1
#define EEPROM_SLAVE_ID 0xA2
// common register
#define SENSOR_ID_ADDR 0x0000
#define FRAME_LENGTH_LINES_ADDR 0x0340
#define FRAME_LENGTH_LINES_SHIFT_ADDR 0x0702
#define LINE_LENGTH_PIXEL_ADDR 0x0342
#define COARSE_INTEGRATION_TIME_ADDR 0x0202
#define COARSE_INTEGRATION_TIME_SHIFT_ADDR 0x0704
#define AGAIN_ADDR 0x0204
#define DGAIN_ADDR 0x020E
#define GROUP_HOLD_ADDR 0x0104
#define AWB_R_GAIN_ADDR 0x0D82
#define AWB_G_GAIN_ADDR 0x0D84
#define AWB_B_GAIN_ADDR 0x0D86

#define LOG_TAG "s5khp3sunny"
#define S5KHP3SUNNY_LOG_INF(format, args...) pr_err(LOG_TAG "[%s] " format, __func__, ##args)
#define S5KHP3SUNNY_LOG_DBG(format, args...) pr_debug(LOG_TAG "[%s] " format, __func__, ##args)
/*N6 code for HQ-306077 by HQ camera at 20230713 start*/
#define SUNNY_VENDOR_ID 0x01
/*N6 code for HQ-306077 by HQ camera at 20230713 end*/

#define QSC_DATA_LEN 9216
static unsigned char s5khp3_QSC_data[QSC_DATA_LEN];



static DEFINE_SPINLOCK(imgsensor_drv_lock);
static bool bIsLongExposure = KAL_FALSE;
static kal_uint32 sensor_version;
/*N6 code for HQ-342415 by HQ camera at 20231102 start*/
static kal_bool is_streamon = KAL_FALSE;
/*N6 code for HQ-342415 by HQ camera at 20231102 end*/
static kal_bool is_isz_4x = KAL_FALSE;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KHP3_SUNNY_MAIN_SENSOR_ID,		//record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0x47a75476,		//checksum value for Camera Auto Test

	.pre = { /*4080*3072 30fps*/
		.pclk = 2000000000,
		.linelength  = 10560,
		.framelength = 6304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.cap = {/*4080*3072 30fps 10bit 12.5M 30fps */
		.pclk = 2000000000,
		.linelength  = 10560,
		.framelength = 6304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.normal_video = {/*10bit 4080x2296 30fps */
		.pclk = 2000000000,
		.linelength  = 10560,
		.framelength = 6304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.hs_video = {/*slow motion 1920*1080p 120fps*/
		.pclk = 2000000000,
		.linelength = 6040,
		.framelength = 2758,
 		.startx = 0,
 		.starty = 0,
 		.grabwindow_width = 1920,
 		.grabwindow_height = 1080,
 		.mipi_data_lp2hs_settle_dc = 0x22,
 		.max_framerate = 1200,
		.mipi_pixel_rate = 1357056000,
	},
	.slim_video = {/*4080*2296 60fps*/
		.pclk = 2000000000,
		.linelength  = 10560,
		.framelength = 3152,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 600,
		.mipi_pixel_rate = 1357056000,
	},
	.custom1 = {/*4080*3072 24fps stereo same as preview */
		.pclk = 2000000000,
		.linelength  = 14080,
		.framelength = 5822,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 244,
		.mipi_pixel_rate = 1357056000,
	},
	.custom2 = {/*4080*2296@30fps 12bit IDCG*/ 
		.pclk = 2000000000,
		.linelength  = 11520,
		.framelength = 5776,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
		.raw_bit = 12,
	},
	.custom3 = {/*4080*3072 30fps sensor zoom4x*/
		.pclk = 2000000000,
		.linelength = 20336,
		.framelength = 3276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.custom4 = {/*8160*6144 15fps full size capture*/
		.pclk = 1760000000,
		.linelength  = 12320,
		.framelength = 9456,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8160,
		.grabwindow_height = 6144,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 150,
		.mipi_pixel_rate = 1357056000,
	},
	.custom5 = {/*4080*3072 30fps sensor zoom2x*/
		.pclk = 1760000000,
		.linelength  = 11616,
		.framelength = 5040,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.custom6 = {/*4080*3072 24.4fps sensor zoom2x*/
		.pclk = 1760000000,
		.linelength  = 12416,
		.framelength = 5810,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 244,
		.mipi_pixel_rate = 1357056000,
	},
	.custom7 = {/*1920*1080 30fps for pipe mode*/
		.pclk = 2000000000,
		.linelength = 6656,
		.framelength = 10016,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 300,
		.mipi_pixel_rate = 1357056000,
	},
	.custom8 = {//full size capture
		.pclk = 2000000000,
		.linelength  = 26560,
		.framelength = 12496,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 16320,
		.grabwindow_height = 12288,
		.mipi_data_lp2hs_settle_dc = 0x22,
		.max_framerate = 60,
		.mipi_pixel_rate = 1357056000,
	},/*16320*12288 6fps sensor full size*/

	.margin = 55,			//sensor framelength & shutter margin
	.min_shutter = 16,		//min shutter
	.exp_step    = 2,
	.min_gain = BASEGAIN, // 1x gain
	.max_gain = 128 * BASEGAIN, // real again is 128x
	.min_gain_iso = 50,
	.gain_step = 1,
	.gain_type = 2,
	.max_frame_length = 0xFFFF,//max framelength by sensor register's limitation
	.ae_shut_delay_frame        = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame     = 2, /* isp gain delay frame for AE cycle */
	.frame_time_delay_frame     = 2, // n+1 effect ,need set 2
	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 0, /* 1, support; 0,not support */
	.sensor_mode_num            = 13,/* support sensor mode num */
	.cap_delay_frame        = 2, /* enter capture delay frame num */
	.pre_delay_frame        = 1, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2,
	.slim_video_delay_frame = 2, /* enter slim video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2, /* enter custom2 delay frame num */
	.custom3_delay_frame    = 1, /* enter custom3 delay frame num */
	.custom4_delay_frame    = 2, /* enter custom4 delay frame num */
	.custom5_delay_frame    = 1, /* enter custom5 delay frame num */
	.custom6_delay_frame    = 2, /* enter custom6 delay frame num */
	.custom7_delay_frame	= 2, /* enter custom7 delay frame num */
	.custom8_delay_frame	= 2, /* enter custom8 delay frame num */
	.isp_driving_current      = ISP_DRIVING_6MA,
	.sensor_interface_type    = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type         = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode   = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.mclk           = 24,   /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num  = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed      = 1000, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x4C00,					//current shutter
	.gain = 0x200,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
	.current_ae_effective_frame = 2,
	.is_writen_otp_done = KAL_FALSE,
};

/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[13] = {
	{ 16320,12288,    0,    0,  16320,12288,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // Preview 12.5M 30fps
	{ 16320,12288,    0,    0,  16320,12288,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // capture 12.5M 30fps
	{ 16320,12288,    0,  1552,  16320, 9184,  4080, 2296,    0,    0,  4080, 2296,    0,    0,  4080, 2296}, // normal video 4080x2296 30fps
	{ 16320,12288, 2400,  2904,  11520, 6480,  1920, 1080,    0,    0,  1920, 1080,    0,    0,  1920, 1080}, // hsvideo as 120fps
	{ 16320,12288,    0,  1552,  16320, 9184,  4080, 2296,    0,    0,  4080, 2296,    0,    0,  4080, 2296}, // slimvideo 60fps
	{ 16320,12288,    0,    0,  16320,12288,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // custom1 stereo as Preview 24fps
	{ 16320,12288,    0,  1552,  16320, 9184,  4080, 2296,    0,    0,  4080, 2296,    0,    0,  4080, 2296}, // custom2 12.5M 30fpsIDCG video 4080x2296
	{ 16320,12288, 6120,  4608,   4080, 3072,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // custom3 12.5M 30fps sensor zoom4x
	{ 16320,12288,    0,    0,  16320,12288,  8160, 6144,    0,    0,  8160, 6144,    0,    0,  8160, 6144}, // custom4 50M 15fps
	{ 16320,12288, 4080,  3072,   8160, 6144,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // custom5 12.5M 30fps sensor zoom2x
	{ 16320,12288, 4080,  3072,   8160, 6144,  4080, 3072,    0,    0,  4080, 3072,    0,    0,  4080, 3072}, // custom6 12.5M 24fps sensor zoom2x
	{ 16320,12288, 2400,  2904,  11520, 6480,  1920, 1080,    0,    0,  1920, 1080,    0,    0,  1920, 1080}, // custom7 1080p 30fps
	{ 16320,12288,    0,     0, 16320,12288,  16320,12288,    0,    0,  16320,12288,    0,    0,  16320,12288}, // custom8 16320*12288 6fps sensor full size
};

#if ENABLE_PDAF
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	/*O6 code for HQ-412023 by liuchaochao at 20240830 begin*/
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 388}, {1080, 996}, {0, 388},
		{0, 0}, {60, 228}, {6120, 4608}, {0, 0}, {2040, 1536},
		{2040, 1536}, {1080, 996},
	},//{0, 1632}
	/*O6 code for HQ-412023 by liuchaochao at 20240830 end*/
	.iMirrorFlip = 0,
};
#endif

static struct SENSOR_VC_INFO2_STRUCT SENSOR_VC_INFO2[6] = {
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//preview
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0xff0, 0xC00},//4080*3072
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 0x13ec, 0x300},//4080*768
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//video
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0xff0, 0x8f8},//4080*2296
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 0x13ec, 0x23e},//4080*574
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//1080p@120fps
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0x780, 0x438},//1920*1080
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 0x960, 0x10e},//1920*270
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom3@zoom4x
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0xff0, 0xC00},//4080*3072
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 0x4f8, 0xc0},//1272*192
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom5@zoom2x
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 0xff0, 0xC00},//4080*3072
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 0x9f8, 0x180},//2552*384
		},
		1
	},
	{
		0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,//custom2 12bit video
		{
			{VC_STAGGER_NE, 0x00, 0x2c, 0xff0, 0x8f8},//4080*2296
			{VC_PDAF_STATS_NE_PIX_1, 0x01, 0x30, 4080, 574},//4080*574
		},
		1
	},
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

	return get_byte;
}

static kal_uint16 read_cmos_sensor_twobyte(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char get_word[2] = { 0, 0};
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	/* iReadRegI2C(pu_send_cmd, 2, get_word, 2, imgsensor.i2c_write_id); */
	iReadRegI2CTiming(pu_send_cmd, 2, get_word, 2, imgsensor.i2c_write_id,
			  imgsensor_info.i2c_speed);
	get_byte = (((int)get_word[0]) << 8) | get_word[1];
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF)
	};
	iWriteRegI2CTiming(pu_send_cmd, 4, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

/*N6 code for HQ-306077 by HQ camera at 20230713 start*/
static kal_uint16 get_vendor_id(void)
{
        kal_uint16 get_byte = 0;
        char pusendcmd[2] = {(char)(0x01 >> 8), (char)(0x01 & 0xFF) };

        iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA2);
        return get_byte;
}
/*N6 code for HQ-306077 by HQ camera at 20230713 end*/

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;
/*N6 code for HQ-306077 by HQ camera at 20230713 start*/
	kal_uint32 vendor_id = 0;

	sensor_id = ((read_cmos_sensor(SENSOR_ID_ADDR) << 8) | read_cmos_sensor(SENSOR_ID_ADDR + 1)) +1;
	sensor_version = ((read_cmos_sensor(0x000f)<<8)|read_cmos_sensor(0x0010));

	vendor_id = get_vendor_id();
	if(vendor_id != SUNNY_VENDOR_ID)
	{
		S5KHP3SUNNY_LOG_INF("not match vendor id:0x%x\n",vendor_id);
		sensor_id = 0xFFFF;//temp change for bringup
	}

	S5KHP3SUNNY_LOG_INF("[%s] vendor_id: 0x%x, sensor_id: 0x%x,sensor_version:0x%x\n", __func__, vendor_id, sensor_id,sensor_version);
/*N6 code for HQ-306077 by HQ camera at 20230713 end*/
	return sensor_id;
}

static kal_uint32 get_sensor_temperature(void)
{
	kal_uint32 temperature = 0;
	kal_uint32 temperature_convert = 0;

	temperature = read_cmos_sensor_twobyte(0x40000020);
	temperature_convert = temperature/256;

	S5KHP3SUNNY_LOG_INF("temp_c(%d) 'C, read_reg(0x%x)\n", temperature_convert, temperature);

	return temperature_convert;
}

static void set_dummy(void)
{
	S5KHP3SUNNY_LOG_DBG("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
//	write_cmos_sensor(GROUP_HOLD_ADDR, 0x1);
//	write_cmos_sensor(LINE_LENGTH_PIXEL_ADDR, imgsensor.line_length & 0xFFFF);
//	write_cmos_sensor(FRAME_LENGTH_LINES_ADDR, imgsensor.frame_length & 0xFFFF);
//	write_cmos_sensor(GROUP_HOLD_ADDR, 0x0);

}	/*	set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	S5KHP3SUNNY_LOG_DBG("framerate = %d, min framelength =%d, should enable? \n", framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
		//imgsensor.dummy_line = 0;
	//else
		//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	if(shutter >= 68900)
	{
		/*enter long exposure mode */
		kal_uint32 exposure_time;
		kal_uint16 new_framelength;
		kal_uint16 long_shutter=0;

		bIsLongExposure = KAL_TRUE;
		exposure_time = shutter*imgsensor.line_length/(imgsensor_info.pre.pclk / 1000 - 1000);
		long_shutter = shutter / 128 ;
		LOG_INF("enter lvy long exposure mode long_shutter = %d, long_exposure_time =%u ms\n", long_shutter, exposure_time);
		new_framelength = long_shutter + 24;

		write_cmos_sensor(0x0340, new_framelength);
		write_cmos_sensor(0x0202, long_shutter);
		write_cmos_sensor(0x0702, 0x0700);
		write_cmos_sensor(0x0704, 0x0700);

		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 2;
	}
	else
	{
		imgsensor.current_ae_effective_frame = 2;
		if (bIsLongExposure)
		{
			LOG_INF("enter normal shutter.\n");
			write_cmos_sensor(0xFCFC, 0x4000);
			write_cmos_sensor(0x0340, imgsensor.frame_length);
			write_cmos_sensor(0x0702, 0x0000);
			write_cmos_sensor(0x0704, 0x0000);
			write_cmos_sensor(0x0202, shutter);
			bIsLongExposure = KAL_FALSE;
		}

		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = shutter + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length = imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (shutter < imgsensor_info.min_shutter)
			shutter = imgsensor_info.min_shutter;

		if (imgsensor.autoflicker_en)
		{
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
			if(realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296,0);
			else if(realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146,0);
			else
				/* Extend frame length*/
				write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
		else
		{
			/* Extend frame length*/
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}

		/* Update Shutter*/
		write_cmos_sensor(0x0202, shutter);
	}
	S5KHP3SUNNY_LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}				/*      set_shutter */

static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length, kal_bool auto_extend_en)
{
	unsigned long flags;
	kal_int32 dummy_line = 0;
	kal_uint16 Rshift;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin) {
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	}
	/*else {
		imgsensor.frame_length = imgsensor.min_frame_length;
	}*/

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(FRAME_LENGTH_LINES_ADDR, imgsensor.frame_length & 0xFFFF);
		}
	}

	if (shutter & 0xFFFF0000) {
		Rshift = 6;
	}
	else {
		Rshift = 0;
	}

	// Update Shutter
	write_cmos_sensor(GROUP_HOLD_ADDR, 0x00);
	write_cmos_sensor(FRAME_LENGTH_LINES_ADDR, (imgsensor.frame_length >> Rshift) & 0xFFFF);
	write_cmos_sensor(COARSE_INTEGRATION_TIME_ADDR, (shutter >> Rshift) & 0xFFFF);
	write_cmos_sensor(FRAME_LENGTH_LINES_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(COARSE_INTEGRATION_TIME_SHIFT_ADDR, (Rshift << 8) & 0xFFFF);
	write_cmos_sensor(GROUP_HOLD_ADDR, 0x01);

	S5KHP3SUNNY_LOG_DBG("set shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

#define FACTOR 992.0f//(15.5f　* 64.0f)
static kal_uint32 digital_gain_calc(kal_uint16 aaa_gain)
{
	float real_dig_gain = 1.0f;//MIN Dgain
	kal_uint32 reg_dig_gain = 1024;//1024 = 1x
	kernel_neon_begin();
	real_dig_gain = aaa_gain / FACTOR;
	if (real_dig_gain > 15.99f) {
		real_dig_gain = 15.99f;
	}
	reg_dig_gain = (kal_uint32)(real_dig_gain * 1024) << 6;
	kernel_neon_end();
	return reg_dig_gain;
}

#if ENABLE_PDAF
static void set_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{

	MUINT32 r_Gain = pSetSensorAWB->ABS_GAIN_R;
	MUINT32 g_Gain = pSetSensorAWB->ABS_GAIN_GR;
	MUINT32 b_Gain = pSetSensorAWB->ABS_GAIN_B;

	write_cmos_sensor(AWB_R_GAIN_ADDR, r_Gain * 2);
	write_cmos_sensor(AWB_G_GAIN_ADDR, g_Gain * 2);
	write_cmos_sensor(AWB_B_GAIN_ADDR, b_Gain * 2);

	if (is_isz_4x) {
		write_cmos_sensor(0xFCFC, 0x2001);
		write_cmos_sensor(0x93A4, r_Gain * 2);
		write_cmos_sensor(0x93A6, b_Gain * 2);
		write_cmos_sensor(0xFCFC, 0x4000);
	}

    S5KHP3SUNNY_LOG_INF("write awb gain r:g:b %d:%d:%d \n", r_Gain, g_Gain, b_Gain);
}
#endif

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
	kal_uint32 reg_dig_gain;
	kal_uint16 max_gain = imgsensor_info.max_gain;
	S5KHP3SUNNY_LOG_INF("set_gain %d \n", gain);
	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		S5KHP3SUNNY_LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}
    // sensor reg gain = real_gain * 0x20 / BASEGAIN
	reg_gain = gain / 2;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	S5KHP3SUNNY_LOG_DBG("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    /*N6 code for HQ-311155 by wangjie at 2023/9/21 start*/
	if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM4 ||
	    imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM3 ||
	    imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM5 ||
		imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM6) {

		// full mode max AGain is 16x
		max_gain = 64 * BASEGAIN;
	}else if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM8 ||
	    imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
                max_gain = 16 * BASEGAIN;
        }
    /*N6 code for HQ-311155 by wangjie at 2023/9/21 end*/
	if (gain > max_gain) {
		reg_dig_gain = digital_gain_calc(gain);
		write_cmos_sensor(AGAIN_ADDR, max_gain / 2);
		write_cmos_sensor(DGAIN_ADDR, (0x0100 & 0xFFFF));
	} else {
		write_cmos_sensor(AGAIN_ADDR, reg_gain);
		write_cmos_sensor(DGAIN_ADDR, 0x0100); // 1x digital Gain
	}

	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	S5KHP3SUNNY_LOG_DBG("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length = imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;

		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;

		// Extend frame length first
		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 8) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 16) & 0xFF);

		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);

		set_gain(gain);
	}
}

/*N6 code for HQ-342415 by HQ camera at 20231102 start*/
static void s5khp3sunny_set_curr_lens_data(u8 *para, u32 *len)
{
/*N6 code for HQ-343533 by HQ camera at 20231103 start*/
	UINT32 *data = (UINT32*)para;
	unsigned short lp = (unsigned short)(*data);
/*N6 code for HQ-343533 by HQ camera at 20231103 end*/
	if (is_streamon) {
		write_cmos_sensor(0xFCFC, 0x4000);
/*O6 code for HQ-404760 by xiexinli at 20240902 start*/
		write_cmos_sensor(0x0B0E, lp);
/*O6 code for HQ-404760 by xiexinli at 20240902 end*/
		S5KHP3SUNNY_LOG_INF("%s lp:0x%x\n", __func__, lp);
	} else
		S5KHP3SUNNY_LOG_INF("%s sensor is not stream on, lp:0x%x\n", __func__, lp);
}
/*N6 code for HQ-342415 by HQ camera at 20231102 end*/

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
}	/*	night_mode	*/

/*N6 code for HQ-309652 by HQ camera at 20230729 start*/
static kal_uint32 streaming_control(struct imgsensor_struct imgsensor,kal_bool enable)
{
	int timeout = imgsensor.current_fps ? (10000 / imgsensor.current_fps) + 1 : 101 ;
	int i=0;
	int framecnt = 0;

	S5KHP3SUNNY_LOG_INF("streaming_enable(0=sw standby,1=streaming):%d\n",enable);
	if (enable) {
		is_streamon = KAL_TRUE;
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor_8(0x0100, 0x01);
		mdelay(10);
	} else {
		is_streamon = KAL_FALSE;
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor_8(0x0100, 0x00);
		for(i=0; i < timeout; i++)
		{
			mdelay(5);
			framecnt = read_cmos_sensor(0x0005);
			if(framecnt == 0xFF){
				printk("stream off at i=%d\n",i);
				return ERROR_NONE;
			}
		}
		S5KHP3SUNNY_LOG_INF("stream off fail! framecnt = %d\n",framecnt);
	}
	 return ERROR_NONE;
}
/*N6 code for HQ-309652 by HQ camera at 20230729 end*/

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 4
#endif
/*N6 code for HQ-325253 by HQ camera at 20230907 start*/
static kal_uint16 table_write_cmos_sensor(kal_uint16 *para,
					  kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	unsigned int  tosend, IDX;
	unsigned short  addr, data, addr_last;
	unsigned short  single_addr = 0;
	unsigned short  single_data = 0;
	tosend = 0;
	IDX = 0;
	while(IDX < len)
	{
		addr = para[IDX];
		if(tosend == 0)
		{
			addr_last = addr - 2;
		}
		else
		{
			addr_last = para[IDX - 2];
		}

		if (tosend == 0)
		{
			single_addr = addr;
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX+1];
			single_data = data;
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
		}
		else if (addr == (addr_last + 2))
		{
			data = para[IDX+1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
		}
		// to send out the data if the sen buffer is full or last data or to program to the different address.
		if ((tosend == I2C_BUFFER_LEN) || (IDX == len) || (addr != (addr_last + 2)))
		{
			if(tosend < 5)
			{
				iWriteRegI2CTiming(puSendCmd, 4,
					imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
			}
			else
			{
				iBurstWriteReg_multi(puSendCmd,
							tosend,
							imgsensor.i2c_write_id,
							tosend,
							imgsensor_info.i2c_speed);
			}
			tosend = 0;
		}
	}

	return 0;
}
/*N6 code for HQ-325253 by HQ camera at 20230907 end*/

static void sensor_init(void)
{
	write_cmos_sensor(0xFCFC,0x4000);
	write_cmos_sensor(0x0000,0x0009);
	write_cmos_sensor(0x0000,0x1B73);
	write_cmos_sensor(0x6012,0x0001);
	write_cmos_sensor(0x7002,0x0008);
	write_cmos_sensor(0x6014,0x0001);
	mdelay(10);

	// Global
	S5KHP3SUNNY_LOG_INF("loading global setting+\n");
	table_write_cmos_sensor(s5khp3sunny_init_setting_global,
			sizeof(s5khp3sunny_init_setting_global) / sizeof(kal_uint16));
	S5KHP3SUNNY_LOG_INF("loading global setting-\n");
}	/*	sensor_init  */

static void preview_setting(void)
{
	S5KHP3SUNNY_LOG_INF("+\n");
	table_write_cmos_sensor(s5khp3sunny_preview_setting,
			sizeof(s5khp3sunny_preview_setting) / sizeof(kal_uint16));

	S5KHP3SUNNY_LOG_INF("-\n");
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	S5KHP3SUNNY_LOG_INF("capture setting E\n");
	table_write_cmos_sensor(s5khp3sunny_capture_setting,
			sizeof(s5khp3sunny_capture_setting) / sizeof(kal_uint16));
}	/*	capture_setting  */


static void normal_video_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E!\n");
	table_write_cmos_sensor(s5khp3sunny_normal_video_setting,
			sizeof(s5khp3sunny_normal_video_setting) / sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");

	table_write_cmos_sensor(s5khp3sunny_hs_video_setting,
		sizeof(s5khp3sunny_hs_video_setting) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_slim_video_setting,
		sizeof(s5khp3sunny_slim_video_setting) / sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom1_setting,
		sizeof(s5khp3sunny_custom1_setting) / sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_idcg_12bit_setting,
			sizeof(s5khp3sunny_idcg_12bit_setting) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");

	table_write_cmos_sensor(s5khp3sunny_custom3_setting,
			sizeof(s5khp3sunny_custom3_setting) / sizeof(kal_uint16));
}

static void custom4_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom4_setting,
			sizeof(s5khp3sunny_custom4_setting) / sizeof(kal_uint16));
}

static void custom5_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom5_setting,
			sizeof(s5khp3sunny_custom5_setting) / sizeof(kal_uint16));
}

static void custom6_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom6_setting,
			sizeof(s5khp3sunny_custom6_setting) / sizeof(kal_uint16));
}

static void custom7_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom7_setting,
			sizeof(s5khp3sunny_custom7_setting) / sizeof(kal_uint16));
}

static void custom8_setting(void)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	table_write_cmos_sensor(s5khp3sunny_custom8_setting,
			sizeof(s5khp3sunny_custom8_setting) / sizeof(kal_uint16));
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
//extern void app_get_front_sensor_name(char *back_name);
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 3;
	S5KHP3SUNNY_LOG_INF("S5KHP3SUNNY get_imgsensor_id in\n");
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				// read_cali_data_from_eeprom();//read calibration data
				S5KHP3SUNNY_LOG_INF("probe success, i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				//read_QSC_data();

				return ERROR_NONE;
			} else {
				S5KHP3SUNNY_LOG_INF("Read sensor id fail,i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 3;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

#if 0
static unsigned char read_cmos_eeprom_8(kal_uint16 addr)
{
	unsigned char get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, &get_byte, 1, 0xA2);
	return get_byte;
}

static void read_QSC_data(void)
{
	kal_uint16 idx = 0;
	kal_uint16 addr_qsc_flag = 0x57E5;
	kal_uint16 addr_qsc = 0x57E6;
	kal_uint32 read_qsc_len_1 = 0;
	kal_uint32 read_qsc_len_2 = 0;
	char pusendcmd[2] = {(char)(addr_qsc >> 8), (char)(addr_qsc & 0xFF) };

	read_qsc_len_1 = 64 - (addr_qsc % 64);
	S5KHP3SUNNY_LOG_INF("read_QSC_data E");
	if(1 == read_cmos_eeprom_8(addr_qsc_flag))
	{
		iReadRegI2C(pusendcmd, 2, &s5khp3_QSC_data[0], read_qsc_len_1, EEPROM_SLAVE_ID);
		pusendcmd[0] = (char)((addr_qsc + read_qsc_len_1) >> 8);
		pusendcmd[1] = (char)((addr_qsc + read_qsc_len_1) & 0xFF);

		/*lot size Read eeprom QSC data*/
		for (idx = 0; idx < (QSC_DATA_LEN - read_qsc_len_1) / 64; idx++)
		{
			iReadRegI2C(pusendcmd, 2, &s5khp3_QSC_data[read_qsc_len_1 + read_qsc_len_2], 64, EEPROM_SLAVE_ID);
			read_qsc_len_2 += 64;
			pusendcmd[0] = (char)((addr_qsc + read_qsc_len_1 + read_qsc_len_2) >> 8);
			pusendcmd[1] = (char)((addr_qsc + read_qsc_len_1 + read_qsc_len_2) & 0xFF);
		}

		iReadRegI2C(pusendcmd, 2, &s5khp3_QSC_data[QSC_DATA_LEN-((QSC_DATA_LEN - read_qsc_len_1) % 64)], (QSC_DATA_LEN - read_qsc_len_1) % 64, EEPROM_SLAVE_ID);

	}else {
		S5KHP3SUNNY_LOG_INF("read_QSC_data error invalid flag");
	}

	for (idx = 0; idx < QSC_DATA_LEN; idx++)
	{
		S5KHP3SUNNY_LOG_DBG("read_QSC_data[%d] = 0x%x",idx,s5khp3_QSC_data[idx]);
	}
	S5KHP3SUNNY_LOG_INF("read_QSC_data X");

}

#endif



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
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 3;
	kal_uint16 sensor_id = 0;
	S5KHP3SUNNY_LOG_INF("PLATFORM:MT6877, MIPI 3LANE\n");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				S5KHP3SUNNY_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			S5KHP3SUNNY_LOG_INF("Read sensor id fail,i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 3;
	}
	if ((imgsensor_info.sensor_id != sensor_id))
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x2D00;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.is_writen_otp_done = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */

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
	S5KHP3SUNNY_LOG_INF("E\n");

	/*No Need to implement this function*/
	streaming_control(imgsensor,KAL_FALSE);

	return ERROR_NONE;
}	/*	close  */

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
	S5KHP3SUNNY_LOG_INF("loading preview_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}	/*	preview   */

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
	S5KHP3SUNNY_LOG_INF("loading capture_setting\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}	/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading normal_video_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();

	return ERROR_NONE;
}	/*	normal_video   */


static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading hs_video_setting\n");

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

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("E\n");

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

	return ERROR_NONE;
}	/*	slim_video	 */

static kal_uint32 custom1(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom1_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();

	return ERROR_NONE;
}	/*      custom1       */

static kal_uint32 custom2(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom2_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();

	return ERROR_NONE;
}	/*      custom2       */

static kal_uint32 custom3(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom3_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();

	return ERROR_NONE;
}	/*      custom3       */

static kal_uint32 custom4(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom4_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();

	return ERROR_NONE;
}	/*      custom4       */

static kal_uint32 custom5(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom5_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();

	return ERROR_NONE;
}	/*      custom5       */

static kal_uint32 custom6(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom6_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	imgsensor.pclk = imgsensor_info.custom6.pclk;
	imgsensor.line_length = imgsensor_info.custom6.linelength;
	imgsensor.frame_length = imgsensor_info.custom6.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom6_setting();

	return ERROR_NONE;
}	/*      custom6       */

	static kal_uint32 custom7(
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
	{
		S5KHP3SUNNY_LOG_INF("loading custom7_setting\n");
	
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM7;
		imgsensor.pclk = imgsensor_info.custom7.pclk;
		imgsensor.line_length = imgsensor_info.custom7.linelength;
		imgsensor.frame_length = imgsensor_info.custom7.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom7.framelength;
		imgsensor.dummy_line = 0;
		imgsensor.dummy_pixel = 0;
		imgsensor.autoflicker_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		custom7_setting();
	
		return ERROR_NONE;
	}	/*		custom7 	  */
static kal_uint32 custom8(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("loading custom8_setting\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM8;
	imgsensor.pclk = imgsensor_info.custom8.pclk;
	imgsensor.line_length = imgsensor_info.custom8.linelength;
	imgsensor.frame_length = imgsensor_info.custom8.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom8.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom8_setting();

	return ERROR_NONE;
}	/*      custom8       */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	S5KHP3SUNNY_LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;

	sensor_resolution->SensorCustom6Width = imgsensor_info.custom6.grabwindow_width;
	sensor_resolution->SensorCustom6Height = imgsensor_info.custom6.grabwindow_height;

	sensor_resolution->SensorCustom7Width = imgsensor_info.custom7.grabwindow_width;
	sensor_resolution->SensorCustom7Height = imgsensor_info.custom7.grabwindow_height;

	sensor_resolution->SensorCustom8Width = imgsensor_info.custom8.grabwindow_width;
	sensor_resolution->SensorCustom8Height = imgsensor_info.custom8.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
	sensor_info->Custom6DelayFrame = imgsensor_info.custom6_delay_frame;
	sensor_info->Custom7DelayFrame = imgsensor_info.custom7_delay_frame;
	sensor_info->Custom8DelayFrame = imgsensor_info.custom8_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

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
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_QPD;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

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
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		sensor_info->SensorGrabStartX = imgsensor_info.custom6.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom6.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		sensor_info->SensorGrabStartX = imgsensor_info.custom7.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom7.starty;
		
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom7.mipi_data_lp2hs_settle_dc;
		
		break;
	case MSDK_SCENARIO_ID_CUSTOM8:
		sensor_info->SensorGrabStartX = imgsensor_info.custom8.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom8.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom8.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5KHP3SUNNY_LOG_INF("scenario_id = %d\n", scenario_id);
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
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		custom6(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		custom7(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM8:
		custom8(image_window, sensor_config_data);
		break;
	default:
		S5KHP3SUNNY_LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */

//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id, kal_uint32 shutter, kal_uint32 gain,kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	imgsensor.extend_frame_length_en = KAL_FALSE;
	S5KHP3SUNNY_LOG_INF("seamless switch to %d!\n",scenario_id);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	{
		mdelay(30);
		S5KHP3SUNNY_LOG_INF("seamless switch to preview! gain(%d), shutter(%d)\n", gain, shutter);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.pre.pclk;
		imgsensor.line_length = imgsensor_info.pre.linelength;
		imgsensor.frame_length = imgsensor_info.pre.framelength;
		imgsensor.min_frame_length = imgsensor_info.pre.framelength;
		is_isz_4x = KAL_FALSE;
		//imgsensor.extend_frame_length_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		S5KHP3SUNNY_LOG_INF("seamless switch 1-exp!\n");
		preview_setting();
        //JD1 is in AE LOCK stage for development.
		if (gain_2ndframe != 0) {
			set_gain(gain_2ndframe);
		}
		if (shutter != 0) {
			set_shutter(shutter);
		}
	}
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	{
		S5KHP3SUNNY_LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 seamless switch to 4x mode! gain(%d), shutter(%d)\n", gain, shutter);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom3.pclk;
		imgsensor.line_length = imgsensor_info.custom3.linelength;
		imgsensor.frame_length = imgsensor_info.custom3.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
		is_isz_4x = KAL_TRUE;
		//imgsensor.extend_frame_length_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		S5KHP3SUNNY_LOG_INF("seamless switch 4-exp!\n");
		custom3_setting();
		if (gain_2ndframe != 0) {
			set_gain(gain_2ndframe);
		}
		if (shutter != 0) {
			set_shutter(shutter);
		}
	}
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	{
		S5KHP3SUNNY_LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 seamless switch to 2x mode! gain(%d), shutter(%d)\n", gain, shutter);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = scenario_id;
		imgsensor.autoflicker_en = KAL_FALSE;
		imgsensor.pclk = imgsensor_info.custom5.pclk;
		imgsensor.line_length = imgsensor_info.custom5.linelength;
		imgsensor.frame_length = imgsensor_info.custom5.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
		//imgsensor.extend_frame_length_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		S5KHP3SUNNY_LOG_INF("seamless switch 2-exp!\n");
		custom5_setting();
		if (gain_2ndframe != 0) {
			set_gain(gain_2ndframe);
		}
		if (shutter != 0) {
			set_shutter(shutter);
		}
	}
		break;
	default:
		S5KHP3SUNNY_LOG_INF("error! wrong setting in set_seamless_switch = %d", scenario_id);
		return 0xff;
	}
	imgsensor.fast_mode_on = KAL_TRUE;
	S5KHP3SUNNY_LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}
//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end

static kal_uint32 set_video_mode(UINT16 framerate)
{
	S5KHP3SUNNY_LOG_DBG("framerate = %d\n ", framerate);
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
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	S5KHP3SUNNY_LOG_DBG("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	S5KHP3SUNNY_LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			 set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			 set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	//case MSDK_SCENARIO_ID_CAMERA_ZSD:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			 set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			 set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			 set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ? (frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10 / imgsensor_info.custom6.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom6.framelength) ? (frame_length - imgsensor_info.custom6.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom6.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		frame_length = imgsensor_info.custom7.pclk / framerate * 10 / imgsensor_info.custom7.linelength;
		
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom7.framelength) ? (frame_length - imgsensor_info.custom7.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom7.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;
	case MSDK_SCENARIO_ID_CUSTOM8:
		frame_length = imgsensor_info.custom8.pclk / framerate * 10 / imgsensor_info.custom8.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom8.framelength) ? (frame_length - imgsensor_info.custom8.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom8.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_DBG("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		break;

	default:  //coding with  preview scenario by default
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter) {
			set_dummy();
		} else {
			/*No need to set*/
			S5KHP3SUNNY_LOG_INF("frame_length %d < shutter %d", imgsensor.frame_length, imgsensor.shutter);
		}
		S5KHP3SUNNY_LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	S5KHP3SUNNY_LOG_INF("scenario_id = %d\n", scenario_id);

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
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM7:
		*framerate = imgsensor_info.custom7.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM8:
		*framerate = imgsensor_info.custom8.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	S5KHP3SUNNY_LOG_INF("enable: %d\n", enable);
	//N6 code for HQ-324136 by luwei at 2023/09/11 start
	if (enable) {
		write_cmos_sensor(0x0600, 0x0001); /*solid color*/
		write_cmos_sensor(0x0602, 0x0000);
		write_cmos_sensor(0x0604, 0x0000);
		write_cmos_sensor(0x0606, 0x0000);
		write_cmos_sensor(0x0608, 0x0000);
	}
	else{
		write_cmos_sensor(0x0600, 0x0000); /*No pattern*/
	}
	//N6 code for HQ-324136 by luwei at 2023/09/11 end
	/*
	if (enable) {
		// 0x5081[0]: 1 enable,  0 disable
		// 0x5081[5:4]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0600, 0x02);
	} else {
		write_cmos_sensor(0x0600, 0x00);
	}
	*/
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	// unsigned long long *feature_return_para=(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO2_STRUCT *pvcinfo2;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	UINT32  fpsOV;
	UINT32  TimeOV;
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end

#if ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif

	S5KHP3SUNNY_LOG_DBG("feature_id = %d\n", feature_id);
	switch (feature_id) {
//O6 code for HQ-412756, by p-geyanjie1, 20240830, start
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 2038250;////Referring to MTK FAQ21698 ,adjustment only affects sensor_fusion
		break;
//O6 code for HQ-412756, by p-geyanjie1, 20240830, end
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
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom7.framelength << 16)
				+ imgsensor_info.custom7.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom8.framelength << 16)
				+ imgsensor_info.custom8.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
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
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom5.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom6.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom7.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom8.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
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
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
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
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		S5KHP3SUNNY_LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		S5KHP3SUNNY_LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[10], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[11], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM8:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[12], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		S5KHP3SUNNY_LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)(*feature_data), (UINT16)(*(feature_data + 1)), (BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;	//1：表示set_shutter_frame_length 支持第三个参数
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(imgsensor,KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(imgsensor,KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM4:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		default:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		}
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
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

		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80))*
			imgsensor_info.custom2.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80))*
			imgsensor_info.custom3.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80))*
			imgsensor_info.custom4.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80))*
			imgsensor_info.custom5.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom6.pclk /
			(imgsensor_info.custom6.linelength - 80))*
			imgsensor_info.custom6.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom7.pclk /
			(imgsensor_info.custom7.linelength - 80))*
			imgsensor_info.custom7.grabwindow_width;
			
			break;
		case MSDK_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom8.pclk /
			(imgsensor_info.custom8.linelength - 80))*
			imgsensor_info.custom8.grabwindow_width;

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
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		*feature_return_para_32 = imgsensor.current_ae_effective_frame;
		S5KHP3SUNNY_LOG_INF("GET AE EFFECTIVE %d\n", *feature_return_para_32);
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		memcpy(feature_return_para_32, &imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
		S5KHP3SUNNY_LOG_INF("GET_AE_FRAME_MODE");
		break;
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
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom7.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom8.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO2:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);
		pvcinfo2 = (struct SENSOR_VC_INFO2_STRUCT *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
					sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[1],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM7:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[2],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[5],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
				S5KHP3SUNNY_LOG_INF("enter custom2 get vc info 5\n");
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[3],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[4],
				sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		default:
			memcpy((void *)pvcinfo2, (void *)&SENSOR_VC_INFO2[0],
			       sizeof(struct SENSOR_VC_INFO2_STRUCT));
			break;
		}
		break;
#if ENABLE_PDAF

	case SENSOR_FEATURE_GET_PDAF_DATA:
		S5KHP3SUNNY_LOG_INF("odin GET_PDAF_DATA EEPROM\n");
		break;

	case SENSOR_FEATURE_GET_PDAF_INFO:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
		case MSDK_SCENARIO_ID_CUSTOM7:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
/*N6 code for HQHW-4840 by huabinchen at 20230810 start*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
/*N6 code for HQHW-4840 by huabinchen at 20230810 end*/
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		imgsensor.pdaf_mode = *feature_data_16;
		S5KHP3SUNNY_LOG_INF("[odin] pdaf mode : %d \n", imgsensor.pdaf_mode);
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
/*N6 code for HQ-330628 by HQ camera at 20231025 start*/
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
/*N6 code for HQ-330628 by HQ camera at 20231025 end*/
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		set_awb_gain((struct SET_SENSOR_AWB_GAIN *) feature_para);
		break;
#endif
 	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
  		*feature_return_para_32 = get_sensor_temperature();
  		*feature_para_len = 4;
  		break;
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, start
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_SEAMLESS_SWITCH");
		switch (*feature_data)
		{
		case MSDK_SCENARIO_ID_CUSTOM3:
			fpsOV = imgsensor_info.custom3.pclk/imgsensor.frame_length/imgsensor_info.custom3.linelength;
			TimeOV = 1000/fpsOV;
			mdelay(TimeOV);
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			fpsOV = imgsensor_info.custom5.pclk/imgsensor.frame_length/imgsensor_info.custom5.linelength;
			TimeOV = 1000/fpsOV;
			mdelay(TimeOV);
			break;
		default:
			S5KHP3SUNNY_LOG_INF("fpsOV feature_data(%llu)", *feature_data);
			break;
		}
		if ((feature_data + 1) != NULL)
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		else
			S5KHP3SUNNY_LOG_INF("warning! no ae_ctrl input");
		if (feature_data == NULL) {
			S5KHP3SUNNY_LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}
		S5KHP3SUNNY_LOG_INF("call seamless_switch");
		if (pAeCtrls != NULL) {
			S5KHP3SUNNY_LOG_INF("aectrlcall seamless_switch_arctrl");
			seamless_switch((*feature_data),*pAeCtrls, *(pAeCtrls + 1), *(pAeCtrls + 4), *(pAeCtrls + 5));
		} else {
			S5KHP3SUNNY_LOG_INF("call seamless_switch_null");
			seamless_switch((*feature_data), 0, 0, 0, 0);
	}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL) {
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data + 1)));
			S5KHP3SUNNY_LOG_INF("feature_data(%llu), *pScenarios(%d)", *feature_data, *pScenarios);
		}
		else
		{
			S5KHP3SUNNY_LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data)
		{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*pScenarios = MSDK_SCENARIO_ID_CUSTOM5;//9
			*(pScenarios + 1) = MSDK_SCENARIO_ID_CUSTOM3;//7
			S5KHP3SUNNY_LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW to MSDK_SCENARIO_ID_CUSTOM5 + MSDK_SCENARIO_ID_CUSTOM3");
			break;
		case MSDK_SCENARIO_ID_CUSTOM3://4x
		case MSDK_SCENARIO_ID_CUSTOM5://2x
		case MSDK_SCENARIO_ID_CUSTOM8: //full size
			*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;//0
			S5KHP3SUNNY_LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 or MSDK_SCENARIO_ID_CUSTOM5 to MSDK_SCENARIO_ID_CAMERA_PREVIEW\n");
			break;
		default:
			*pScenarios = 0xff;
			break;
		}
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %llu %d\n", *feature_data, *pScenarios);
		break;
	//N6 code for HQ-305655, by p-zhaobeidou3, 20230906, end
	case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
	{
		int type = (kal_uint16)(*feature_data);
		char *data = (char *)(uintptr_t)(*(feature_data+1));
		S5KHP3SUNNY_LOG_INF("SENSOR_FEATURE_GET_4CELL_DATA");

		if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
			data[0] = 0x00;
			data[1] = 0x24;
			memcpy(data + 2, s5khp3_QSC_data, QSC_DATA_LEN);
			S5KHP3SUNNY_LOG_INF("Read FOUR_CELL_CAL_TYPE_XTALK_CAL = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);
		}else if (type == FOUR_CELL_CAL_TYPE_DPC){
			data[0] = 0x80;
			data[1] = 0x95;
			S5KHP3SUNNY_LOG_INF("Read FOUR_CELL_CAL_TYPE_DPC = %02x %02x %02x %02x %02x %02x\n",
				(UINT16)data[0], (UINT16)data[1],
				(UINT16)data[2], (UINT16)data[3],
				(UINT16)data[4], (UINT16)data[5]);

		}
		break;
	}
/*N6 code for HQ-342415 by HQ camera at 20231102 start*/
	case SENSOR_FEATURE_SET_CURR_LENS_DATA:
		s5khp3sunny_set_curr_lens_data((u8 *)feature_data, feature_para_len);
	break;
/*N6 code for HQ-342415 by HQ camera at 20231102 end*/
	//get raw bit set
	case SENSOR_FEATURE_GET_RAW_BIT_BY_SCENARIO:
	{
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.raw_bit;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.raw_bit;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.raw_bit;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2://IDCG
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM6:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM7:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CUSTOM8:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom8.raw_bit;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CUSTOM9:
			case MSDK_SCENARIO_ID_CUSTOM10:
			case MSDK_SCENARIO_ID_CUSTOM11:
			case MSDK_SCENARIO_ID_CUSTOM12:
			case MSDK_SCENARIO_ID_CUSTOM13:
			case MSDK_SCENARIO_ID_CUSTOM14:
			case MSDK_SCENARIO_ID_CUSTOM15:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.raw_bit;
			break;
		}
		pr_err("IDCG [%s] get mode(%llu) raw bit(%d)\n",
		__func__, *feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	//get brightness:
	case SENSOR_FEATURE_GET_SATURATION_LEVEL_BY_SCENARIO:
	{
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 4092;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:		
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_CUSTOM6:
			case MSDK_SCENARIO_ID_CUSTOM7:
			case MSDK_SCENARIO_ID_CUSTOM8:
			case MSDK_SCENARIO_ID_CUSTOM9:
			case MSDK_SCENARIO_ID_CUSTOM10:
			case MSDK_SCENARIO_ID_CUSTOM11:
			case MSDK_SCENARIO_ID_CUSTOM12:
			case MSDK_SCENARIO_ID_CUSTOM13:
			case MSDK_SCENARIO_ID_CUSTOM14:
			case MSDK_SCENARIO_ID_CUSTOM15:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= 0;
			break;
		}
		pr_err("IDCG SENSOR_FEATURE_GET_SATURATION_LEVEL_BY_SCENARIO %llu %d\n",
			*feature_data, *(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	}
	//get STAGGER_TARGET:
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
	{
		if (*feature_data == MSDK_SCENARIO_ID_VIDEO_PREVIEW) { //2
			switch (*(feature_data + 1)) {
			case HDR_RAW_DCG_COMPOSE_RAW12://13
				*(feature_data + 2) = MSDK_SCENARIO_ID_CUSTOM2;//11
				break;
			default:
			    break;
			}
		} else if (*feature_data == MSDK_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = MSDK_SCENARIO_ID_VIDEO_PREVIEW;
				break;
			default:
			    break;
			}
		}
		pr_err("IDCG SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO %d %d %d\n",
			(UINT16)(*feature_data),
			(UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		break;
	}

	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KHP3_SUNNY_MAIN_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    S5KHP3SUNNY_LOG_INF("S5KHP3_SUNNY_MAIN_MIPI_RAW_SensorInit in\n");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	S5KHP3_SUNNY_MAIN_MIPI_RAW_SensorInit	*/
