/*****************************************************************************
 *
 * Copyright:
 * ---------
 * Copyright (C), 2023-2024, MetaSilicon Tech. Co., Ltd.
 * All rights reserved.
 *
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "mt815_jk_front_iiii_mipi_raw_Sensor.h"

#define PFX "mt815_camera_sensor"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define MAX_GAIN 240
#define MIN_GAIN 0
//#define MIPI_2LANE

#ifdef MT815_LONG_EXP
#define MAX_CIT_LSHIFT 7
#endif

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 765
#else
#define I2C_BUFFER_LEN 3
#endif

#ifdef MT_SENSOR_CUSTOMIZED
static struct IMGSENSOR_LOAD_FROM_FILE *gp_load_file = NULL;
#endif

#if 1
extern unsigned char fusion_id_front[96];
extern unsigned char sn_front[96];
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = MT815_JK_FRONT_IIII_SENSOR_ID,
    .checksum_value = 0x9948d9d8,
    .pre = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .cap = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .normal_video = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 1836,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .hs_video = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .slim_video = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
     },
    .custom1 = {
        .pclk = 132000000,
        .linelength = 1738,
        .framelength = 2530,
#ifdef MIPI_2LANE
        .mipi_pixel_rate = 264000000,
#else
        .mipi_pixel_rate = 288000000,
#endif
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1632,
        .grabwindow_height = 1224,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
     },

    .min_gain = 64,   // 1.125x, base 64
    .max_gain = 1024,  // 16x
    .min_gain_iso = 100,
    .gain_step = 4,
    .gain_type = 2,

    .margin = 6,
    .min_shutter = 2,
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .frame_time_delay_frame = 3,
    .ihdr_support = 0,                                       // 1:support; 0:not support
    .ihdr_le_firstline = 0,                                  // 1:le first ; 0:se first
    .sensor_mode_num = 6,                                    // support sensor mode num

    .cap_delay_frame = 3,
    .pre_delay_frame = 3,
    .video_delay_frame = 3,
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,
    .custom1_delay_frame = 3,

    .isp_driving_current = ISP_DRIVING_4MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2,                      // 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,          // 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
    .mclk = 24,
#ifdef MIPI_2LANE
    .mipi_lane_num = SENSOR_MIPI_2_LANE,
#else
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
#endif
    .i2c_addr_table = {0x40,0x42,0x44,0x46,0xff},
    .i2c_speed = 400,
};

static  imgsensor_struct imgsensor = {
    .mirror = IMAGE_HV_MIRROR,                                     // mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT,                         // IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                                           // current shutter
    .gain = 0x100,                                              // current gain
    .dummy_pixel = 0,                                           // current dummypixel
    .dummy_line = 0,                                            // current dummyline
    .current_fps = 0,                                           // full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,                                // auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,                                  // test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,     // current scenario id
    .ihdr_mode = 0,                                             // sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x40,
#ifdef MT815_LONG_EXP
    .current_ae_effective_frame = 2,
#endif
};

/* Sensor output window information */
/*for ex:slim video configure, should to be cut in the same way as the sensor
1 2 3 4 5 6 column:3264x2448 crop w:3264-0=3264 h:2448-2*504=1440
5 6 7 8 column:3264*1440 2binning 1632*720
9 10 11 12 column:1280*720 crop w:1632-2*176=1280 h:720-0=720
13 14 15 16 column:1280*720 not need crop*/
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] =
{
    {3264, 2448, 0,     0, 3264, 2448, 3264, 2448,   0, 0, 3264, 2448, 0, 0, 3264, 2448}, // Preview
    {3264, 2448, 0,     0, 3264, 2448, 3264, 2448,   0, 0, 3264, 2448, 0, 0, 3264, 2448}, // capture
    {3264, 2448, 0,   306, 3264, 1836, 3264, 1836,   0, 0, 3264, 1836, 0, 0, 3264, 1836}, // video
    {3264, 2448, 0,     0, 3264, 2448, 3264, 2448,   0, 0, 3264, 2448, 0, 0, 3264, 2448}, // hight speed video
    {3264, 2448, 0,     0, 3264, 2448, 3264, 2448,   0, 0, 3264, 2448, 0, 0, 3264, 2448}, // slim video
    {3264, 2448, 0, 	0, 3264, 2448, 1632, 1224,   0, 0, 1632, 1224, 0, 0, 1632, 1224}  //custom1
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
    LOG_INF("mt815 read_cmos_sensor addr is 0x%x, val is 0x%x", addr, get_byte);
    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    LOG_INF("mt815 write_cmos_sensor addr is 0x%x, val is 0x%x", addr, para);
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

#if 1
static kal_uint16 read_cmos_sensor_mt815_jk_otp(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, 0xA0);
	return get_byte;
}

static void mt815_jk_fusion_id_read(void)
{
	int i;
	for (i=0; i<17; i++) {
		fusion_id_front[i] = read_cmos_sensor_mt815_jk_otp(0x10+i);
	pr_err("%s %d zengx fusion_id_front[%d]=0x%2x\n",__func__, __LINE__, i, fusion_id_front[i]);
	}
}

static void mt815_jk_sn_id_read(void)
{
	int i;
	for (i=0; i<25; i++) {
	        sn_front[i] = read_cmos_sensor_mt815_jk_otp(0x1F87+i);
		pr_err("%s %d zengx sn_front[%d]=0x%2x\n",__func__, __LINE__, i, sn_front[i]);
	}
}
#endif

static void set_dummy(void)
{
    LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
    write_cmos_sensor(0x0104, 1);
    write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
    write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
    write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
    write_cmos_sensor(0x0104, 0);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;
    LOG_INF("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if(frame_length >= imgsensor.min_frame_length)
        imgsensor.frame_length = frame_length;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    // imgsensor.dummy_line = dummy_line;
    // imgsensor.frame_length = frame_length + imgsensor.dummy_line;
    if(imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if(min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}

static void update_shutter_frame_reg(kal_uint8 exp_mode, kal_uint8 long_exp_time, kal_uint32 shutter, kal_uint32 frame_length)
{
    write_cmos_sensor(0x0104, 0x01);
#ifdef MT815_LONG_EXP
    write_cmos_sensor(0x0236, exp_mode & 0xFF);
    write_cmos_sensor(0x0235, long_exp_time & 0xFF);
#endif
    write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8)  & 0xFF);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF);
    write_cmos_sensor(0x0104, 0x00);
    LOG_INF("shutter =%d, framelength =%d\n", shutter, frame_length);
}

static void write_shutter(kal_uint32 shutter)
{
#ifdef MT815_LONG_EXP
    kal_uint8 exp_mode = 0;
    kal_uint8 l_shift = 0;
    kal_uint8 long_exp_time = 0;
#endif
    kal_uint16 realtime_fps = 0;

    spin_lock(&imgsensor_drv_lock);
    if(shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if(imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    if(shutter < imgsensor_info.min_shutter)
        shutter = imgsensor_info.min_shutter;

    realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
    if(imgsensor.autoflicker_en)
    {
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else
            LOG_INF("autoflicker enable do not need change framerate\n");
    }

#ifdef MT815_LONG_EXP
    /* long expsoure */
    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
    {
        for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
            if ((shutter >> l_shift)  < (imgsensor_info.max_frame_length - imgsensor_info.margin))
                break;
        }
        if (l_shift > MAX_CIT_LSHIFT) {
            LOG_INF("unable to set such a long exposure %d\n", shutter);
            l_shift = MAX_CIT_LSHIFT;
        }
        exp_mode = 0x01;
        long_exp_time = 0x08 | l_shift;
        shutter = shutter >> l_shift;
        imgsensor.frame_length = shutter + imgsensor_info.margin;
        LOG_INF("enter long exposure mode, time is %d", l_shift);

        // Update frame length and shutter of long exposure mode
        update_shutter_frame_reg(exp_mode, long_exp_time, shutter, imgsensor.frame_length);

        /* Frame exposure mode customization for LE*/
        imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
        imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
    }
    else
#endif
    {
        // Update frame length and shutter of normal mode
        update_shutter_frame_reg(0, 0, shutter, imgsensor.frame_length);
    }
#ifdef MT815_LONG_EXP
    imgsensor.current_ae_effective_frame = 2;
#endif

    LOG_INF("shutter =%d, framelength =%d, real_fps =%d\n", shutter, imgsensor.frame_length, realtime_fps);
}

/*************************************************************************
* FUNCTION
*       set_shutter
*
* DESCRIPTION
*       This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*       iShutter : exposured lines
*
* RETURNS
*       None
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
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 val = MIN_GAIN;

    if (gain < imgsensor_info.min_gain)
        val = MIN_GAIN;
    else if (gain > imgsensor_info.max_gain)
        val = MAX_GAIN;
    else
        val = (kal_uint16)(gain * (MAX_GAIN - MIN_GAIN) / (imgsensor_info.max_gain - imgsensor_info.min_gain)) - 16;

    LOG_INF("plantform gain=%d, sensor gain= 0x%x\n", gain, val);
    return val;
}

/*************************************************************************
* FUNCTION
*       set_gain
*
* DESCRIPTION
*       This function is to set global gain to sensor.
*
* PARAMETERS
*       iGain : sensor global gain(base: 0x40)
*
* RETURNS
*       the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    if (gain < BASEGAIN)
    {
        LOG_INF("error gain setting = %d, base gain = %d", gain, BASEGAIN);
        gain = BASEGAIN;
    }
    if (gain > 16 * BASEGAIN)
    {
        LOG_INF("error gain setting = %d, max gain = %d", gain, 16 * BASEGAIN);
        gain = 16 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = 4 * reg_gain + 64;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d, reg_gain = 0x%x\n", gain, reg_gain);

    write_cmos_sensor(0x0104, 1);
    write_cmos_sensor(0x0205, reg_gain);
    write_cmos_sensor(0x0104, 0);

    return gain;
}

/*************************************************************************
* FUNCTION
*       night_mode
*
* DESCRIPTION
*       This function night mode of sensor.
*
* PARAMETERS
*       bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
}

static kal_uint16 mt815_table_write_cmos_sensor(
                    kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend = 0, IDX = 0;
    kal_uint16 addr = 0, addr_last = 0, data = 0;

    while (len > IDX) {
        addr = para[IDX];
        {
            if (addr == 0xFFFF)
            {
                data = para[IDX + 1];
                mdelay(data);
                IDX += 2;
                LOG_INF("sleep time = %d, IDX = %d", data, IDX);
            }
            else
            {
                puSendCmd[tosend++] = (char)(addr >> 8);
                puSendCmd[tosend++] = (char)(addr & 0xFF);
                data = para[IDX + 1];
                puSendCmd[tosend++] = (char)(data & 0xFF);
                IDX += 2;
                addr_last = addr;
            }
        }

    #if MULTI_WRITE
        /* Write when remain buffer size is less than
        *3 bytes or reach end of data*/
        if ((I2C_BUFFER_LEN - tosend) < 3
            || IDX == len
            || addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd,
                tosend,
                imgsensor.i2c_write_id,
                3,
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

#ifdef MIPI_2LANE
kal_uint16 addr_data_pair_init_mt815[] = {
    0x0d00, 0x00,
    0x0d01, 0x00,
    0x0d02, 0x86,
    0x0d03, 0x21,
    0x0d04, 0x18,
    0x0d05, 0x4c,
    0x0d06, 0x06,
    0x0d07, 0x05,
    0x0d08, 0x04,
    0x0d09, 0x00,
    0x0d10, 0x21,
    0x0d11, 0x2c,
    0x0d12, 0x21,
    0x0d13, 0x02,
    0x0d14, 0x11,
    0x0d15, 0x11,
    0x0d16, 0x03,
    0x0d17, 0x00,
    0x0d18, 0x14,
    0x0d19, 0x23,
    0x0d1a, 0x37,
    0x0d1b, 0x62,
    0x0d1c, 0x3c,
    0x0d1d, 0x0c,
    0xffff, 0x01,
    0x0008, 0x01,
    0x0009, 0x11,
    0x0018, 0x03,
    0x0019, 0x20,
    0x001b, 0x01,
    0x001c, 0xaa,
    0x001d, 0x50,
    0x0101, 0x03,
    0x0220, 0x00,
    0x0230, 0x01,
    0x0231, 0x00,
    0x0232, 0x01,
    0x0233, 0x00,
    0x0234, 0x00,
    0x0238, 0x01,
    0x0239, 0x00,
    0x023a, 0x18,
    0x023b, 0x00,
    0x023c, 0x08,
    0x0241, 0x00,
    0x0242, 0x08,
    0x024a, 0x02,
    0x024d, 0x00,
    0x024e, 0x00,
    0x024f, 0xe4,
    0x0269, 0x00,
    0x026a, 0x00,
    0x026b, 0x01,
    0x026d, 0x1f,
    0x0275, 0x40,
    0x0e00, 0x00,
    0x0e01, 0x00,
    0x0e02, 0x01,
    0x0e03, 0xb8,
    0x0e04, 0x02,
    0x0e05, 0x00,
    0x0e06, 0x01,
    0x0e07, 0x70,
    0x0e08, 0x01,
    0x0e09, 0x00,
    0x0e0d, 0x01,
    0x0e18, 0x00,
    0x0e20, 0x00,
    0x0e0a, 0x00,
    0x0e0c, 0x70,
    0x024b, 0x04,
    0x0e0e, 0x01,
    0x0243, 0x07,
    0x0244, 0x01,
    0x0245, 0x07,
    0x0246, 0x03,
    0x0247, 0x06,
    0x0248, 0x03,
    0x0249, 0x06,
    0x0250, 0x00,
    0x0251, 0x01,
    0x0252, 0x73,
    0x0253, 0x01,
    0x0254, 0xd8,
    0x0255, 0x00,
    0x0256, 0x17,
    0x0257, 0x00,
    0x0258, 0x47,
    0x0259, 0x01,
    0x025a, 0x75,
    0x025b, 0x01,
    0x025c, 0xc4,
    0x025d, 0x04,
    0x025e, 0xda,
    0x025f, 0x05,
    0x0260, 0x29,
    0x0261, 0x08,
    0x0262, 0x3f,
    0x0263, 0x08,
    0x0264, 0x8e,
    0x0265, 0x0b,
    0x0266, 0xa4,
    0x0267, 0x0b,
    0x0268, 0xf3,
    0x0e60, 0x00,
    0x0e61, 0xff,
    0x0e62, 0x0f,
    0x0e63, 0xff,
    0x0e64, 0x0f,
    0x0e65, 0x00,
    0x0e66, 0xff,
    0x0e67, 0x0f,
    0x0e68, 0xff,
    0x0e69, 0x0f,
    0x0e6a, 0x01,
    0x0e6b, 0x88,
    0x0e6c, 0x00,
    0x0e6d, 0x54,
    0x0e6e, 0x03,
    0x0e6f, 0x00,
    0x0e70, 0xac,
    0x0e71, 0x00,
    0x0e72, 0x72,
    0x0e73, 0x01,
    0x0e74, 0xea,
    0x0e75, 0x01,
    0x0e76, 0x54,
    0x0e77, 0x03,
    0x0e78, 0x01,
    0x0e79, 0xfc,
    0x0e7a, 0x00,
    0x0e7b, 0x72,
    0x0e7c, 0x01,
    0x0e7d, 0x3a,
    0x0e7e, 0x02,
    0x0e7f, 0x54,
    0x0e80, 0x03,
    0x0e81, 0x01,
    0x0e82, 0xfc,
    0x0e83, 0x00,
    0x0e84, 0x72,
    0x0e85, 0x01,
    0x0e86, 0x3a,
    0x0e87, 0x02,
    0x0e88, 0x54,
    0x0e89, 0x03,
    0x0e8a, 0x01,
    0x0e8b, 0x88,
    0x0e8c, 0x00,
    0x0e8d, 0x58,
    0x0e8e, 0x03,
    0x0e8f, 0x00,
    0x0e90, 0x88,
    0x0e91, 0x00,
    0x0e92, 0x58,
    0x0e93, 0x03,
    0x0e94, 0x00,
    0x0e95, 0xac,
    0x0e96, 0x00,
    0x0e97, 0xb1,
    0x0e98, 0x00,
    0x0e99, 0x72,
    0x0e9a, 0x01,
    0x0e9b, 0x77,
    0x0e9c, 0x01,
    0x0e9d, 0xea,
    0x0e9e, 0x01,
    0x0e9f, 0xef,
    0x0ea0, 0x01,
    0x0ea1, 0x54,
    0x0ea2, 0x03,
    0x0ea3, 0x59,
    0x0ea4, 0x03,
    0x0ea5, 0x01,
    0x0ea6, 0x82,
    0x0ea7, 0x00,
    0x0ea8, 0x56,
    0x0ea9, 0x03,
    0x0eaa, 0x00,
    0x0eab, 0x82,
    0x0eac, 0x00,
    0x0ead, 0x56,
    0x0eae, 0x03,
    0x0eaf, 0x01,
    0x0eb0, 0xac,
    0x0eb1, 0x00,
    0x0eb2, 0x54,
    0x0eb3, 0x03,
    0x0eb4, 0x01,
    0x0eb5, 0xa6,
    0x0eb6, 0x00,
    0x0eb7, 0x54,
    0x0eb8, 0x03,
    0x0eb9, 0x00,
    0x0eba, 0xad,
    0x0ebb, 0x00,
    0x0ebc, 0xbb,
    0x0ebd, 0x00,
    0x0ebe, 0xeb,
    0x0ebf, 0x01,
    0x0ec0, 0xf9,
    0x0ec1, 0x01,
    0x0ec2, 0x01,
    0x0ec3, 0xa1,
    0x0ec4, 0x00,
    0x0ec5, 0x54,
    0x0ec6, 0x03,
    0x0ec7, 0x00,
    0x0ec8, 0xa0,
    0x0ec9, 0x00,
    0x0eca, 0x54,
    0x0ecb, 0x03,
    0x0ecc, 0x01,
    0x0ecd, 0xf2,
    0x0ece, 0x00,
    0x0ecf, 0x71,
    0x0ed0, 0x01,
    0x0ed1, 0x30,
    0x0ed2, 0x02,
    0x0ed3, 0x53,
    0x0ed4, 0x03,
    0x0ed5, 0x00,
    0x0ed6, 0xde,
    0x0ed7, 0x00,
    0x0ed8, 0xe8,
    0x0ed9, 0x00,
    0x0eda, 0x1c,
    0x0edb, 0x02,
    0x0edc, 0x26,
    0x0edd, 0x02,
    0x0ede, 0x01,
    0x0edf, 0xb5,
    0x0ee0, 0x01,
    0x0ee1, 0xb8,
    0x0ee2, 0x01,
    0x0ee3, 0x5f,
    0x0ee4, 0x03,
    0x0ee5, 0x62,
    0x0ee6, 0x03,
    0x0ee7, 0x00,
    0x0ee8, 0xb6,
    0x0ee9, 0x01,
    0x0eea, 0xb8,
    0x0eeb, 0x01,
    0x0eec, 0x60,
    0x0eed, 0x03,
    0x0eee, 0x62,
    0x0eef, 0x03,
    0x0ef0, 0x00,
    0x0ef1, 0x16,
    0x0ef2, 0x00,
    0x0ef3, 0xb0,
    0x0ef4, 0x01,
    0x0ef5, 0xc0,
    0x0ef6, 0x01,
    0x0ef7, 0x5a,
    0x0ef8, 0x03,
    0x0ef9, 0x00,
    0x0efa, 0xff,
    0x0efb, 0x0f,
    0x0efc, 0xff,
    0x0efd, 0x0f,
    0x0efe, 0xff,
    0x0eff, 0x0f,
    0x0f00, 0xff,
    0x0f01, 0x0f,
    0x0f02, 0x00,
    0x0f03, 0xff,
    0x0f04, 0x0f,
    0x0f05, 0xff,
    0x0f06, 0x0f,
    0x0f07, 0xff,
    0x0f08, 0x0f,
    0x0f09, 0xff,
    0x0f0a, 0x0f,
    0x0f0b, 0xff,
    0x0f0c, 0x0f,
    0x0f0d, 0xff,
    0x0f0e, 0x0f,
    0x0f0f, 0xff,
    0x0f10, 0x0f,
    0x0f11, 0xff,
    0x0f12, 0x0f,
    0x0f13, 0x00,
    0x0f14, 0xff,
    0x0f15, 0x0f,
    0x0f16, 0xff,
    0x0f17, 0x0f,
    0x0f18, 0xff,
    0x0f19, 0x0f,
    0x0f1a, 0xff,
    0x0f1b, 0x0f,
    0x0f1c, 0x00,
    0x0f1d, 0xff,
    0x0f1e, 0x0f,
    0x0f1f, 0xff,
    0x0f20, 0x0f,
    0x0f21, 0xff,
    0x0f22, 0x0f,
    0x0f23, 0xff,
    0x0f24, 0x0f,
    0x0f25, 0xff,
    0x0f26, 0x0f,
    0x0f27, 0xff,
    0x0f28, 0x0f,
    0x0f29, 0xff,
    0x0f2a, 0x0f,
    0x0f2b, 0xff,
    0x0f2c, 0x0f,
    0x0f2d, 0xff,
    0x0f2e, 0x0f,
    0x0f2f, 0xff,
    0x0f30, 0x0f,
    0x0f31, 0xff,
    0x0f32, 0x0f,
    0x0f33, 0xff,
    0x0f34, 0x0f,
    0x0f35, 0xff,
    0x0f36, 0x0f,
    0x0f37, 0xff,
    0x0f38, 0x0f,
    0x0f39, 0xff,
    0x0f3a, 0x0f,
    0x0f3b, 0xff,
    0x0f3c, 0x0f,
    0x0403, 0x01,
    0x0404, 0x00,
    0x0421, 0x01,
    0x0422, 0x02,
    0x0423, 0x60,
    0x0424, 0x02,
    0x0425, 0x60,
    0x0426, 0x0a,
    0x0427, 0x02,
    0x0428, 0xff,
    0x0429, 0x00,
    0x042a, 0x30,
    0x042b, 0x10,
    0x042c, 0x01,
    0x042d, 0x01,
    0x0432, 0x01,
    0x0433, 0x00,
    0x0434, 0x00,
    0x0435, 0x00,
    0x0436, 0x01,
    0x043a, 0x01,
    0x043b, 0x01,
    0x043c, 0x01,
    0x0c00, 0x01,
    0x0480, 0x00,
    0x0486, 0x20,
    0x0487, 0x00,
    0x0488, 0xff,
    0x0489, 0x01,
    0x048a, 0x3f,
    0x048b, 0x01,
    0xffff, 0x01,
    0x0c00, 0x00,
    0x0484, 0x01,
    0x0485, 0x01,
    0x055f, 0x01,
    0x0a2d, 0xd9,
    0xffff, 0x01,
    0x0a01, 0x00,
    0x0a02, 0x00,
    0x0a03, 0xe4,
    0x0a04, 0x03,
    0x0a0a, 0x01,
    0x0a0b, 0x2b,
    0x0a10, 0xd8,
    0x0a11, 0x40,
    0x0a12, 0xec,
    0x0a13, 0x84,
    0x0a14, 0x02,
    0x0a15, 0x12,
    0x0a16, 0x0a,
    0x0a17, 0x12,
    0x0a18, 0x08,
    0x0a19, 0x2d,
    0x0a1a, 0x03,
    0x0a1b, 0x09,
    0x0a1c, 0x14,
    0x0a1d, 0x0c,
    0x0a1e, 0x14,
    0x0a1f, 0x00,
    0x0a20, 0x00,
    0x0a21, 0x00,
    0x0a2b, 0x43,
    0x0a2c, 0xc1,
};
#else // MIPI 4LANE
kal_uint16 addr_data_pair_init_mt815[] = {
    0x0d00, 0x00,
    0x0d01, 0x00,
    0x0d02, 0x86,
    0x0d03, 0x21,
    0x0d04, 0x18,
    0x0d05, 0x4c,
    0x0d06, 0x06,
    0x0d07, 0x05,
    0x0d08, 0x04,
    0x0d09, 0x00,
    0x0d10, 0x21,
    0x0d11, 0x2c,
    0x0d12, 0x21,
    0x0d13, 0x02,
    0x0d14, 0x11,
    0x0d15, 0x11,
    0x0d16, 0x03,
    0x0d17, 0x00,
    0x0d18, 0x15,
    0x0d19, 0x03,
    0x0d1a, 0x39,
    0x0d1b, 0x07,
    0x0d1c, 0x3c,
    0x0d1d, 0x0c,
    0xffff, 0x01,
    0x0008, 0x01,
    0x0009, 0x11,
    0x0018, 0x03,
    0x0019, 0x20,
    0x001b, 0x01,
    0x001c, 0xaa,
    0x001d, 0x50,
    0x0101, 0x03,
    0x0220, 0x00,
    0x0230, 0x01,
    0x0231, 0x00,
    0x0232, 0x01,
    0x0233, 0x00,
    0x0234, 0x00,
    0x0238, 0x01,
    0x0239, 0x00,
    0x023a, 0x18,
    0x023b, 0x00,
    0x023c, 0x08,
    0x0241, 0x00,
    0x0242, 0x08,
    0x024a, 0x02,
    0x024d, 0x00,
    0x024e, 0x00,
    0x024f, 0xe4,
    0x0269, 0x00,
    0x026a, 0x00,
    0x026b, 0x01,
    0x026d, 0x1f,
    0x0275, 0x40,
    0x0e00, 0x00,
    0x0e01, 0x00,
    0x0e02, 0x01,
    0x0e03, 0xb8,
    0x0e04, 0x02,
    0x0e05, 0x00,
    0x0e06, 0x01,
    0x0e07, 0x70,
    0x0e08, 0x01,
    0x0e09, 0x00,
    0x0e0d, 0x01,
    0x0e18, 0x00,
    0x0e20, 0x00,
    0x0e0a, 0x00,
    0x0e0c, 0x70,
    0x024b, 0x04,
    0x0e0e, 0x01,
    0x0243, 0x07,
    0x0244, 0x01,
    0x0245, 0x07,
    0x0246, 0x03,
    0x0247, 0x06,
    0x0248, 0x03,
    0x0249, 0x06,
    0x0250, 0x00,
    0x0251, 0x01,
    0x0252, 0x73,
    0x0253, 0x01,
    0x0254, 0xd8,
    0x0255, 0x00,
    0x0256, 0x17,
    0x0257, 0x00,
    0x0258, 0x47,
    0x0259, 0x01,
    0x025a, 0x75,
    0x025b, 0x01,
    0x025c, 0xc4,
    0x025d, 0x04,
    0x025e, 0xda,
    0x025f, 0x05,
    0x0260, 0x29,
    0x0261, 0x08,
    0x0262, 0x3f,
    0x0263, 0x08,
    0x0264, 0x8e,
    0x0265, 0x0b,
    0x0266, 0xa4,
    0x0267, 0x0b,
    0x0268, 0xf3,
    0x0e60, 0x00,
    0x0e61, 0xff,
    0x0e62, 0x0f,
    0x0e63, 0xff,
    0x0e64, 0x0f,
    0x0e65, 0x00,
    0x0e66, 0xff,
    0x0e67, 0x0f,
    0x0e68, 0xff,
    0x0e69, 0x0f,
    0x0e6a, 0x01,
    0x0e6b, 0x88,
    0x0e6c, 0x00,
    0x0e6d, 0x54,
    0x0e6e, 0x03,
    0x0e6f, 0x00,
    0x0e70, 0xac,
    0x0e71, 0x00,
    0x0e72, 0x72,
    0x0e73, 0x01,
    0x0e74, 0xea,
    0x0e75, 0x01,
    0x0e76, 0x54,
    0x0e77, 0x03,
    0x0e78, 0x01,
    0x0e79, 0xfc,
    0x0e7a, 0x00,
    0x0e7b, 0x72,
    0x0e7c, 0x01,
    0x0e7d, 0x3a,
    0x0e7e, 0x02,
    0x0e7f, 0x54,
    0x0e80, 0x03,
    0x0e81, 0x01,
    0x0e82, 0xfc,
    0x0e83, 0x00,
    0x0e84, 0x72,
    0x0e85, 0x01,
    0x0e86, 0x3a,
    0x0e87, 0x02,
    0x0e88, 0x54,
    0x0e89, 0x03,
    0x0e8a, 0x01,
    0x0e8b, 0x88,
    0x0e8c, 0x00,
    0x0e8d, 0x58,
    0x0e8e, 0x03,
    0x0e8f, 0x00,
    0x0e90, 0x88,
    0x0e91, 0x00,
    0x0e92, 0x58,
    0x0e93, 0x03,
    0x0e94, 0x00,
    0x0e95, 0xac,
    0x0e96, 0x00,
    0x0e97, 0xb1,
    0x0e98, 0x00,
    0x0e99, 0x72,
    0x0e9a, 0x01,
    0x0e9b, 0x77,
    0x0e9c, 0x01,
    0x0e9d, 0xea,
    0x0e9e, 0x01,
    0x0e9f, 0xef,
    0x0ea0, 0x01,
    0x0ea1, 0x54,
    0x0ea2, 0x03,
    0x0ea3, 0x59,
    0x0ea4, 0x03,
    0x0ea5, 0x01,
    0x0ea6, 0x82,
    0x0ea7, 0x00,
    0x0ea8, 0x56,
    0x0ea9, 0x03,
    0x0eaa, 0x00,
    0x0eab, 0x82,
    0x0eac, 0x00,
    0x0ead, 0x56,
    0x0eae, 0x03,
    0x0eaf, 0x01,
    0x0eb0, 0xac,
    0x0eb1, 0x00,
    0x0eb2, 0x54,
    0x0eb3, 0x03,
    0x0eb4, 0x01,
    0x0eb5, 0xa6,
    0x0eb6, 0x00,
    0x0eb7, 0x54,
    0x0eb8, 0x03,
    0x0eb9, 0x00,
    0x0eba, 0xad,
    0x0ebb, 0x00,
    0x0ebc, 0xbb,
    0x0ebd, 0x00,
    0x0ebe, 0xeb,
    0x0ebf, 0x01,
    0x0ec0, 0xf9,
    0x0ec1, 0x01,
    0x0ec2, 0x01,
    0x0ec3, 0xa1,
    0x0ec4, 0x00,
    0x0ec5, 0x54,
    0x0ec6, 0x03,
    0x0ec7, 0x00,
    0x0ec8, 0xa0,
    0x0ec9, 0x00,
    0x0eca, 0x54,
    0x0ecb, 0x03,
    0x0ecc, 0x01,
    0x0ecd, 0xf2,
    0x0ece, 0x00,
    0x0ecf, 0x71,
    0x0ed0, 0x01,
    0x0ed1, 0x30,
    0x0ed2, 0x02,
    0x0ed3, 0x53,
    0x0ed4, 0x03,
    0x0ed5, 0x00,
    0x0ed6, 0xde,
    0x0ed7, 0x00,
    0x0ed8, 0xe8,
    0x0ed9, 0x00,
    0x0eda, 0x1c,
    0x0edb, 0x02,
    0x0edc, 0x26,
    0x0edd, 0x02,
    0x0ede, 0x01,
    0x0edf, 0xb5,
    0x0ee0, 0x01,
    0x0ee1, 0xb8,
    0x0ee2, 0x01,
    0x0ee3, 0x5f,
    0x0ee4, 0x03,
    0x0ee5, 0x62,
    0x0ee6, 0x03,
    0x0ee7, 0x00,
    0x0ee8, 0xb6,
    0x0ee9, 0x01,
    0x0eea, 0xb8,
    0x0eeb, 0x01,
    0x0eec, 0x60,
    0x0eed, 0x03,
    0x0eee, 0x62,
    0x0eef, 0x03,
    0x0ef0, 0x00,
    0x0ef1, 0x16,
    0x0ef2, 0x00,
    0x0ef3, 0xb0,
    0x0ef4, 0x01,
    0x0ef5, 0xc0,
    0x0ef6, 0x01,
    0x0ef7, 0x5a,
    0x0ef8, 0x03,
    0x0ef9, 0x00,
    0x0efa, 0xff,
    0x0efb, 0x0f,
    0x0efc, 0xff,
    0x0efd, 0x0f,
    0x0efe, 0xff,
    0x0eff, 0x0f,
    0x0f00, 0xff,
    0x0f01, 0x0f,
    0x0f02, 0x00,
    0x0f03, 0xff,
    0x0f04, 0x0f,
    0x0f05, 0xff,
    0x0f06, 0x0f,
    0x0f07, 0xff,
    0x0f08, 0x0f,
    0x0f09, 0xff,
    0x0f0a, 0x0f,
    0x0f0b, 0xff,
    0x0f0c, 0x0f,
    0x0f0d, 0xff,
    0x0f0e, 0x0f,
    0x0f0f, 0xff,
    0x0f10, 0x0f,
    0x0f11, 0xff,
    0x0f12, 0x0f,
    0x0f13, 0x00,
    0x0f14, 0xff,
    0x0f15, 0x0f,
    0x0f16, 0xff,
    0x0f17, 0x0f,
    0x0f18, 0xff,
    0x0f19, 0x0f,
    0x0f1a, 0xff,
    0x0f1b, 0x0f,
    0x0f1c, 0x00,
    0x0f1d, 0xff,
    0x0f1e, 0x0f,
    0x0f1f, 0xff,
    0x0f20, 0x0f,
    0x0f21, 0xff,
    0x0f22, 0x0f,
    0x0f23, 0xff,
    0x0f24, 0x0f,
    0x0f25, 0xff,
    0x0f26, 0x0f,
    0x0f27, 0xff,
    0x0f28, 0x0f,
    0x0f29, 0xff,
    0x0f2a, 0x0f,
    0x0f2b, 0xff,
    0x0f2c, 0x0f,
    0x0f2d, 0xff,
    0x0f2e, 0x0f,
    0x0f2f, 0xff,
    0x0f30, 0x0f,
    0x0f31, 0xff,
    0x0f32, 0x0f,
    0x0f33, 0xff,
    0x0f34, 0x0f,
    0x0f35, 0xff,
    0x0f36, 0x0f,
    0x0f37, 0xff,
    0x0f38, 0x0f,
    0x0f39, 0xff,
    0x0f3a, 0x0f,
    0x0f3b, 0xff,
    0x0f3c, 0x0f,
    0x0403, 0x01,
    0x0404, 0x00,
    0x0421, 0x01,
    0x0422, 0x02,
    0x0423, 0x60,
    0x0424, 0x02,
    0x0425, 0x60,
    0x0426, 0x0a,
    0x0427, 0x02,
    0x0428, 0xff,
    0x0429, 0x00,
    0x042a, 0x30,
    0x042b, 0x10,
    0x042c, 0x01,
    0x042d, 0x01,
    0x0432, 0x01,
    0x0433, 0x00,
    0x0434, 0x00,
    0x0435, 0x00,
    0x0436, 0x01,
    0x043a, 0x01,
    0x043b, 0x01,
    0x043c, 0x01,
    0x0c00, 0x01,
    0x0480, 0x00,
    0x0486, 0x20,
    0x0487, 0x00,
    0x0488, 0xff,
    0x0489, 0x01,
    0x048a, 0x3f,
    0x048b, 0x01,
    0xffff, 0x01,
    0x0c00, 0x00,
    0x0484, 0x01,
    0x0485, 0x01,
    0x055f, 0x01,
    0x0a2d, 0xd9,
    0xffff, 0x01,
    0x0a01, 0x00,
    0x0a02, 0x00,
    0x0a03, 0xe4,
    0x0a04, 0x0f,
    0x0a0a, 0x01,
    0x0a0b, 0x2b,
    0x0a10, 0x8c,
    0x0a11, 0x23,
    0x0a12, 0xf4,
    0x0a13, 0x5f,
    0x0a14, 0x01,
    0x0a15, 0x0a,
    0x0a16, 0x06,
    0x0a17, 0x0d,
    0x0a18, 0x05,
    0x0a19, 0x19,
    0x0a1a, 0x03,
    0x0a1b, 0x06,
    0x0a1c, 0x0c,
    0x0a1d, 0x07,
    0x0a1e, 0x0c,
    0x0a1f, 0x00,
    0x0a20, 0x00,
    0x0a21, 0x00,
    0x0a2b, 0x43,
    0x0a2c, 0xc1,
    0x0a2f, 0x28,
    0x0a30, 0x28,
    0x0a31, 0x28,
    0x0a32, 0x28,
    0x0a33, 0x28,
};
#endif // MIPI_2LANE

static void sensor_init(void)
{
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->init.size > 0)
    {
        LOG_INF("sensor_init set from file load size=%d\n", gp_load_file->init.size);
        for(i = 0; i < gp_load_file->init.size; i++)
        {
            if(gp_load_file->init.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->init.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->init.reg[i].RegAddr, gp_load_file->init.reg[i].RegData);
        }
        return;
    }
#endif
    mt815_table_write_cmos_sensor(addr_data_pair_init_mt815,
        sizeof(addr_data_pair_init_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_preview_mt815[] = {
    0x0901, 0x00,
    0x0401, 0x03,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x00,
    0x023e, 0x00,
    0x023f, 0x09,
    0x0240, 0x90,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x0c,
    0x040d, 0xc0,
    0x040e, 0x09,
    0x040f, 0x90,
    0x0a0c, 0xc0,
    0x0a0d, 0x0c,
    0x0a0e, 0x90,
    0x0a0f, 0x09,
    0x0406, 0x01,
};

static void preview_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt815
    // Date              : 2024-4-2
    // Image size        : 1920x1080(binning)
    // Frame Length      : 1258
    // Line Length       : 1908
    // Max Fps           : 60.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->preview.size > 0)
    {
        LOG_INF("preview_setting set from file load size=%d\n", gp_load_file->preview.size);
        for(i = 0; i < gp_load_file->preview.size; i++)
        {
            if(gp_load_file->preview.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->preview.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->preview.reg[i].RegAddr, gp_load_file->preview.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("preview_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_preview_mt815,
        sizeof(addr_data_pair_preview_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_capture_mt815[] = {
    0x0901, 0x00,
    0x0401, 0x03,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x00,
    0x023e, 0x00,
    0x023f, 0x09,
    0x0240, 0x90,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x0c,
    0x040d, 0xc0,
    0x040e, 0x09,
    0x040f, 0x90,
    0x0a0c, 0xc0,
    0x0a0d, 0x0c,
    0x0a0e, 0x90,
    0x0a0f, 0x09,
    0x0406, 0x01,
};

static void capture_setting(kal_uint16 currefps)
{
    // Sensor Information////////////////////////////
    // Sensor           : mt815
    // Date             : 2024-4-2
    // Image size       : 3264x2448(full_size)
    // Frame Length     : 2516
    // Line Length      : 1908
    // Pixel order      : Green 1st (=GB)
    // Max Fps          : 30.00fps
    // Pixel order      : Green 1st (=GB)
    // X-mirror/Y-flip  : x-mirror/y-flip
    // BLC offset       : 64code
    ////////////////////////////////////////////////
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->capture.size > 0)
    {
        LOG_INF("capture_setting set from file load size=%d\n", gp_load_file->capture.size);
        for(i = 0; i < gp_load_file->capture.size; i++)
        {
            if(gp_load_file->capture.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->capture.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->capture.reg[i].RegAddr, gp_load_file->capture.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("capture_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_capture_mt815,
        sizeof(addr_data_pair_capture_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_normal_video_mt815[] = {
    0x0901, 0x00,
    0x0401, 0x03,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x01,
    0x023e, 0x32,
    0x023f, 0x07,
    0x0240, 0x2c,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x0c,
    0x040d, 0xc0,
    0x040e, 0x07,
    0x040f, 0x2c,
    0x0a0c, 0xc0,
    0x0a0d, 0x0c,
    0x0a0e, 0x2c,
    0x0a0f, 0x07,
    0x0406, 0x01,
};

static void normal_video_setting(kal_uint16 currefps)
{
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->normal_video.size > 0)
    {
        LOG_INF("normal_video_setting set from file load size=%d\n", gp_load_file->normal_video.size);
        for(i = 0; i < gp_load_file->normal_video.size; i++)
        {
            if(gp_load_file->normal_video.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->normal_video.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->normal_video.reg[i].RegAddr, gp_load_file->normal_video.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("normal_video_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_normal_video_mt815,
        sizeof(addr_data_pair_normal_video_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_hs_video_mt815[] = {
    0x0901, 0x00,
    0x0401, 0x03,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x00,
    0x023e, 0x00,
    0x023f, 0x09,
    0x0240, 0x90,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x0c,
    0x040d, 0xc0,
    0x040e, 0x09,
    0x040f, 0x90,
    0x0a0c, 0xc0,
    0x0a0d, 0x0c,
    0x0a0e, 0x90,
    0x0a0f, 0x09,
    0x0406, 0x01,

};

static void hs_video_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt815
    // Date              : 2024-4-2
    // Image size        : 640x480(bining+crop)
    // Frame Length      : 628
    // Line Length       : 1908
    // Max Fps           : 120.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->hs_video.size > 0)
    {
        LOG_INF("hs_video_setting set from file load size=%d\n", gp_load_file->hs_video.size);
        for(i = 0; i < gp_load_file->hs_video.size; i++)
        {
            if(gp_load_file->hs_video.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->hs_video.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->hs_video.reg[i].RegAddr, gp_load_file->hs_video.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("hs_video_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_hs_video_mt815,
        sizeof(addr_data_pair_hs_video_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_slim_video_mt815[] = {
    0x0901, 0x00,
    0x0401, 0x03,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x00,
    0x023e, 0x00,
    0x023f, 0x09,
    0x0240, 0x90,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x0c,
    0x040d, 0xc0,
    0x040e, 0x09,
    0x040f, 0x90,
    0x0a0c, 0xc0,
    0x0a0d, 0x0c,
    0x0a0e, 0x90,
    0x0a0f, 0x09,
    0x0406, 0x01,

};

static void slim_video_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt815
    // Date              : 2024-4-2
    // Image size        : 3264x2448
    // Frame Length      : 2530
    // Line Length       : 1738
    // Max Fps           : 30.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->slim_video.size > 0)
    {
        LOG_INF("slim_video_setting set from file load size=%d\n", gp_load_file->slim_video.size);
        for(i = 0; i < gp_load_file->slim_video.size; i++)
        {
            if(gp_load_file->slim_video.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->slim_video.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->slim_video.reg[i].RegAddr, gp_load_file->slim_video.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("slim_video_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_slim_video_mt815,
        sizeof(addr_data_pair_slim_video_mt815)/sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_custom1_mt815[] = {
    0x0901, 0x01,
    0x0401, 0x02,
    0x0104, 0x01,
    0x0202, 0x01,
    0x0203, 0x00,
    0x0205, 0x02,
    0x020e, 0x01,
    0x020f, 0x01,
    0x0340, 0x09,
    0x0341, 0xe2,
    0x0342, 0x06,
    0x0343, 0xca,
    0x0235, 0x00,
    0x0236, 0x00,
    0x0237, 0x01,
    0x024c, 0x40,
    0x0104, 0x00,
    0x023d, 0x00,
    0x023e, 0x00,
    0x023f, 0x09,
    0x0240, 0x90,
    0x0408, 0x00,
    0x0409, 0x00,
    0x040a, 0x00,
    0x040b, 0x00,
    0x040c, 0x06,
    0x040d, 0x60,
    0x040e, 0x04,
    0x040f, 0xc8,
    0x0a0c, 0x60,
    0x0a0d, 0x06,
    0x0a0e, 0xc8,
    0x0a0f, 0x04,
    0x0406, 0x01,
};

static void custom1_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt815
    // Date              : 2024-4-2
    // Image size        : 1632x1224
    // Frame Length      : 2530
    // Line Length       : 2738
    // Max Fps           : 30.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
#ifdef MT_SENSOR_CUSTOMIZED
    int i = 0;
    if(gp_load_file && gp_load_file->custom1.size > 0)
    {
        LOG_INF("custom1_setting set from file load size=%d\n", gp_load_file->custom1.size);
        for(i = 0; i < gp_load_file->custom1.size; i++)
        {
            if(gp_load_file->custom1.reg[i].RegAddr == 0xffff)
            {
                mdelay(gp_load_file->custom1.reg[i].RegData);
                continue;
            }
            write_cmos_sensor(gp_load_file->custom1.reg[i].RegAddr, gp_load_file->custom1.reg[i].RegData);
        }
        return;
    }
#endif
    LOG_INF("custom1_setting E\n");
    mt815_table_write_cmos_sensor(addr_data_pair_custom1_mt815,
        sizeof(addr_data_pair_custom1_mt815)/sizeof(kal_uint16));
}

/*************************************************************************
* FUNCTION
*       get_imgsensor_id
*
* DESCRIPTION
*       This function get the sensor ID
*
* PARAMETERS
*       *sensorID : return the sensor ID
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    LOG_INF("mt815 get_imgsensor_id imgsensor_info.sensor_id=0x%x\n", imgsensor_info.sensor_id);
    while(imgsensor_info.i2c_addr_table[i] != 0xff)
    {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do
        {
            mdelay(10);
            *sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (*sensor_id == imgsensor_info.sensor_id)
            {
                LOG_INF("mt815 i2c config id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
#if 1
                mt815_jk_fusion_id_read();
                mt815_jk_sn_id_read();
#endif
                return ERROR_NONE;
            }
            LOG_INF("mt815 Read sensor id fail, config id:0x%x id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 3;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*       open
*
* DESCRIPTION
*       This function initialize the registers of CMOS sensor
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;
#ifdef MIPI_2LANE
    LOG_INF("PLATFORM:MT6877, MIPI 2LANE\n");
#else
    LOG_INF("PLATFORM:MT6877, MIPI 4LANE\n");
#endif

    while (imgsensor_info.i2c_addr_table[i] != 0xff)
    {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do
        {
            mdelay(10);
            sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (sensor_id == imgsensor_info.sensor_id)
            {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id:0x%x id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 3;
    }
    if (imgsensor_info.sensor_id != sensor_id)
            return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();

    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en= KAL_FALSE;
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
*       close
*
* DESCRIPTION
*
*
* PARAMETERS
*       None
*
* RETURNS
*       None
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
*       This function start the sensor preview.
*
* PARAMETERS
*       *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*       None
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
    // imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();

    return ERROR_NONE;
}       /*      preview   */

/*************************************************************************
* FUNCTION
*       capture
*
* DESCRIPTION
*       This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*       None
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
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate)
    {
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    else if(imgsensor.current_fps == imgsensor_info.cap2.max_framerate)
    {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        {
            LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
                imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
        }
        imgsensor.pclk = imgsensor_info.cap2.pclk;
        imgsensor.line_length = imgsensor_info.cap2.linelength;
        imgsensor.frame_length = imgsensor_info.cap2.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    else
    {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        {
            LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
                imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
        }
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);

    return ERROR_NONE;
}

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
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    normal_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    // imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();

    return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    // imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();

    return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    // imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();

    return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight    = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth  = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;

    return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                              MSDK_SENSOR_INFO_STRUCT *sensor_info,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    // sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    // sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
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
    sensor_info->SensorHightSampling = 0;   // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id)
    {
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
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("control scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id)
    {
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
    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable)
        imgsensor.autoflicker_en = KAL_TRUE;
    else
        // Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id)
    {
       case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate)
            {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate)
            {
                frame_length = imgsensor_info.cap2.pclk / framerate * 10 / imgsensor_info.cap2.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap2.framelength) ? (frame_length - imgsensor_info.cap2.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap2.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            else
            {
                if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                {
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
                        framerate,imgsensor_info.cap.max_framerate/10);
                }
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength): 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        default:  // coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    switch (scenario_id)
    {
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

    if (enable)
    {
        //solid dark enable
        write_cmos_sensor(0x0100,0x00);
        write_cmos_sensor(0x0403,0x02);
        write_cmos_sensor(0x05ed,0xc0);
        write_cmos_sensor(0x05ee,0x0c);
        write_cmos_sensor(0x05ef,0x90);
        write_cmos_sensor(0x05f0,0x09);
        write_cmos_sensor(0x05f1,0x52);
        write_cmos_sensor(0x05f2,0x0e);
        write_cmos_sensor(0x05f3,0xff);
        write_cmos_sensor(0x05f4,0xff);
        write_cmos_sensor(0x05f5,0x02);
        write_cmos_sensor(0x05f6,0x02);
        write_cmos_sensor(0x05f7,0x00);
        write_cmos_sensor(0x05f8,0x00);
        write_cmos_sensor(0x05f9,0x00);
        write_cmos_sensor(0x05fa,0x00);
        write_cmos_sensor(0x05fb,0x00);
        write_cmos_sensor(0x05fc,0x00);
        write_cmos_sensor(0x05fd,0x00);
        write_cmos_sensor(0x05fe,0x00);
        write_cmos_sensor(0x0601,0x01);
        write_cmos_sensor(0x0602,0x98);
        write_cmos_sensor(0x0603,0x01);
        write_cmos_sensor(0x0a06,0x00);
        write_cmos_sensor(0x0a0c,0xc0);
        write_cmos_sensor(0x0a0d,0x0c);
        write_cmos_sensor(0x0a0e,0x90);
        write_cmos_sensor(0x0a0f,0x09);
        write_cmos_sensor(0x0100,0x01);
        mdelay(3);
        write_cmos_sensor(0x05ec,0x01);
    }
    else
    {
        write_cmos_sensor(0x05ec,0x00);
        write_cmos_sensor(0x0a06,0x01);
        write_cmos_sensor(0x0403,0x01);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
    pr_info("streaming_enable(0=Sw tandby,1=streaming): %d\n", enable);
    if(enable)
    {
        write_cmos_sensor(0x0100, 0X01);
    }
    else
    {
        write_cmos_sensor(0x0100, 0x00);
    }
    return ERROR_NONE;
}

/*static void set_shutter_frame_length(
                    kal_uint16 shutter, kal_uint16 frame_length,
                    kal_bool auto_extend_en)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
    //Change frame time 
    dummy_line = frame_length - imgsensor.frame_length;
    imgsensor.frame_length = imgsensor.frame_length + dummy_line;
    imgsensor.min_frame_length = imgsensor.frame_length;

    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
              ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if(imgsensor.autoflicker_en)
    {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else
            LOG_INF("autoflicker enable\n");
    }

     // Update frame length and shutter of normal mode
     update_shutter_frame_reg(0, 0, shutter, imgsensor.frame_length);

    LOG_INF("Exit! shutter =%d, imgsensor.framelength =%d, frame_length=%d, dummy_line=%d\n",
            shutter, imgsensor.frame_length, frame_length, dummy_line);
}*/

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                                     UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
#ifdef MT_SENSOR_CUSTOMIZED
    struct IMGSENSOR_LOAD_FROM_FILE *load_val;
#endif
    LOG_INF("mt815  feature_control feature_id = %d\n", feature_id);
    switch (feature_id)
    {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((kal_bool) *feature_data);
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
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((kal_bool)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((kal_bool)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: 
            // for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", (kal_bool)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            //imgsensor.ihdr_mode = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch(*feature_data_32)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CUSTOM1:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
            break;
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            // ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
        case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
            break;
        case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
            pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
            streaming_control(KAL_FALSE);
            break;
        case SENSOR_FEATURE_SET_STREAMING_RESUME:
            pr_info("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
                    *feature_data);
            if (*feature_data != 0)
                set_shutter(*feature_data);
            streaming_control(KAL_TRUE);
            break;
        case SENSOR_FEATURE_GET_BINNING_TYPE:
            switch (*(feature_data + 1)) {
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
            case MSDK_SCENARIO_ID_CUSTOM1:
                *feature_return_para_32 = 1; /*BINNING_NONE*/
                break;
            default:
                *feature_return_para_32 = 2; /*BINNING_AVERAGED*/
                break;
            }
            pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
                *feature_return_para_32);
            *feature_para_len = 4;
            break;
#ifdef MT815_LONG_EXP
        case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
            *feature_return_para_32 = imgsensor.current_ae_effective_frame;
            break;
        case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
            memcpy(feature_return_para_32,
            &imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
            break;
#endif
        case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        {
            switch(*feature_data)
            {
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
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                        imgsensor_info.slim_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_CUSTOM1:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                        imgsensor_info.custom1.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.pre.mipi_pixel_rate;
                    break;
            }
            break;
        }
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
            switch(*feature_data)
            {
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
            break;
        case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
            switch (*feature_data)
            {
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
        case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
            /*
             * 1, if driver support new sw frame sync
             * set_shutter_frame_length() support third para auto_extend_en
            */
            *(feature_data + 1) = 1;
            /* margin info by scenario */
            *(feature_data + 2) = imgsensor_info.margin;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}/*      feature_control()  */

static struct  SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 MT815_JK_FRONT_IIII_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT  **pfFunc)
{
    LOG_INF("MT815_JK_FRONT_IIII_MIPI_RAW_SensorInit\n");
    if (pfFunc!=NULL)
            *pfFunc=&sensor_func;
    return ERROR_NONE;
}
