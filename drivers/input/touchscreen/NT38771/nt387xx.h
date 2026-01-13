/*
 * Copyright (C) 2024 Novatek, Inc.
 *
 * $Revision: 69288 $
 * $Date: 2020-09-23 18:44:22 +0800 (週三, 23 九月 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "nt387xx_mem_map.h"
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_COMMON)
#include "../xiaomi_touch/xiaomi_touch_common.h"
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define BUS_DRIVER_REMOVE_VOID_RETURN
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define SPI_CS_DELAY
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define HAVE_VFS_WRITE
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define reinit_completion(x) INIT_COMPLETION(*(x))
#endif

#ifdef CONFIG_MTK_SPI
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#ifdef CONFIG_SPI_MT65XX
#include <linux/platform_data/spi-mt65xx.h>
#endif
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
#include <linux/pm_qos.h>

#define FUNCPAGE_PALM 4
#define POCKET_PALM_ON 3
#define POCKET_PALM_OFF 4
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_RISING

//---bus transfer length---
#define BUS_TRANSFER_LENGTH 256
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
#define LOCKDOWN_INFO_LENGTH 17
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)

//---Input device info.---
#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_PEN_NAME "NVTCapacitivePen"
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#define NVT_SUPER_RESOLUTION 1 //enable/disable super resolution
#define SUPER_RESOLUTION_FACOTR 16

#if NVT_SUPER_RESOLUTION
#define NVT_DRIVER_INSERT_FRAME 1 //enable/disable insert frame
#define NVT_DRIVER_INSERT_FRAME_TIME 2083333 //480Hz = 2.083333ms
#else
#define NVT_DRIVER_INSERT_FRAME 0
#endif /* #if NVT_SUPER_RESOLUTION */
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
//---Touch info.---
#define TOUCH_MAX_WIDTH 1280
#define TOUCH_MAX_HEIGHT 2772
#define PEN_MAX_WIDTH 1280
#define PEN_MAX_HEIGHT 2772
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_FORCE_NUM 1000
//---for Pen---
#define PEN_PRESSURE_MAX (4095)
#define PEN_DISTANCE_MAX (1)
#define PEN_TILT_MIN (-60)
#define PEN_TILT_MAX (60)

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
#define DEBUG_MAX_BUFFER_SIZE (1024*1024*10)
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define NVT_TOUCH_MP 1
#define NVT_SAVE_TEST_DATA_IN_FILE 0
#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define BOOT_UPDATE_FIRMWARE 1
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
extern char *BOOT_UPDATE_FIRMWARE_NAME;
extern char *MP_UPDATE_FIRMWARE_NAME;
extern unsigned int g_lcm_panel_id;
#define TOUCH_SELECT_0A_CSOT   0x40
#define TOUCH_SELECT_0B_TIANMA 0x10
#define TOUCH_SELECT_0C_VISIONOX 0x30
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65
#define NVT_PM_WAIT_BUS_RESUME_COMPLETE 1
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
#define POINT_DATA_FW_DEBUG_INFO_LEN 384 /* total length need to read, including fw debug info */
#define FW_DEBUG_INFO_OFFSET 0xA0
#define FW_DEBUG_INFO_LEN 224
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/

//---Download firmware boost---
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#define NVT_TRY_SKIP_UPDATE_ILM 1
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define CHECK_PEN_DATA_CHECKSUM 0

#if BOOT_UPDATE_FIRMWARE
#define SIZE_4KB 4096
#define FLASH_SECTOR_SIZE SIZE_4KB
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
#if IS_ENABLED(CONFIG_DRM_PANEL)
#define NVT_DRM_PANEL_NOTIFY 1
#elif IS_ENABLED(_MSM_DRM_NOTIFY_H_)
#define NVT_MSM_DRM_NOTIFY 1
#elif IS_ENABLED(CONFIG_FB)
#define NVT_FB_NOTIFY 1
#elif IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)
#define NVT_EARLYSUSPEND_NOTIFY 1
#endif
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
#define NVT_QCOM_PANEL_EVENT_NOTIFY 1
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK) || IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#define NVT_MTK_DRM_NOTIFY 1
#endif
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#define BTN_INFO      0x152
#define TOUCH_FOD_ID  9
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
extern uint8_t tp_fw_version;
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 start*/
enum NVT_GESTURE_TYP {
	GESTURE_DOUBLE_TAP = (1 << 0),
	GESTURE_SINGLE_TAP = (1 << 1),
	GESTURE_FOD_PRESS  = (1 << 2)
};
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 end*/
struct nvt_ts_data {
	struct spi_device *client;
	struct input_dev *input_dev;
	struct delayed_work nvt_fwu_work;
	uint16_t addr;
	int8_t phys[32];
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	struct notifier_block xiaomi_panel_notif;
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	struct notifier_block drm_notif;
#elif IS_ENABLED(NVT_FB_NOTIFY)
	struct notifier_block fb_notif;
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	struct early_suspend early_suspend;
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
	struct notifier_block disp_notifier;
#endif
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	struct pm_qos_request pm_qos_req_irq;
	bool palm_sensor_changed;
	bool palm_sensor_switch;
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t max_touch_num;
	uint32_t int_trigger_type;
	int32_t irq_gpio;
	int32_t reset_gpio;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint8_t bld_multi_header;
	uint16_t query_config_ver;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	struct work_struct power_supply_work;
	struct workqueue_struct *event_wq;
	struct notifier_block charger_notifier;
	struct work_struct charger_work;
	int charger_status;
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 start*/
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 start*/
	bool fod_rpt_slot_9;
	uint8_t fod_input_id;
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 end*/
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 end*/
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 start*/
	int charger_status_store;
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 end*/
/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 start*/
	int nonui_status;
/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if WAKEUP_GESTURE
	int gesture_command;
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
	bool pen_support;
	bool is_cascade;
	uint8_t pen_x_num_x;
	uint8_t pen_x_num_y;
	uint8_t pen_y_num_x;
	uint8_t pen_y_num_y;
	struct input_dev *pen_input_dev;
	int8_t pen_phys[32];
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t bld_spe_pups_addr;
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 start*/
	u32 gamemode_config[3][4];
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 end*/
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	bool nvt_tool_in_use;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
	bool fw_debug_info_switch;
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 start*/
	uint8_t lockdown[8];
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 end*/
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
	char lockdowninfo[LOCKDOWN_INFO_LENGTH];
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
	char cg_type;
	char vox_cg_type;
#ifdef CONFIG_MTK_SPI
	struct mt_chip_conf spi_ctrl;
#endif
#ifdef CONFIG_SPI_MT65XX
    struct mtk_chip_config spi_ctrl;
#endif
#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
	bool dev_pm_suspend;
	struct completion dev_pm_resume_completion;
#endif
#if NVT_DRIVER_INSERT_FRAME
	struct hrtimer nvt_hrtimer;
	ktime_t kt_delay;
	uint8_t input_event_state[TOUCH_MAX_FINGER_NUM];
	uint32_t pre_fw_input_x[TOUCH_MAX_FINGER_NUM];
	uint32_t pre_fw_input_y[TOUCH_MAX_FINGER_NUM];
#endif
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 start */
	int fod_value;
	int aod_value;
	int doubletap_value;
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 start*/
	int fod_setting;
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 end*/
/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 start */
	int fodicon_value;
/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 end */
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
	int ic_state;
	int gesture_command_delay;
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 end */
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	bool fod_finger;
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
#ifndef CONFIG_FACTORY_BUILD
	struct workqueue_struct *nvt_reset_fw_status_wq;
	struct work_struct nvt_reset_fw_status_work;
	struct completion nvt_window_period_completion;
	struct workqueue_struct *nvt_window_period_wait_wq;
	struct delayed_work nvt_window_period_wait_work;
#endif
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/
};
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
enum NVT_IC_STATE_TYPE {
	NVT_STATE_INIT = 0,
	NVT_STATE_SUSPEND_IN = 1,
	NVT_STATE_SUSPEND_OUT = 2,
	NVT_STATE_RESUME_IN = 3,
	NVT_STATE_RESUME_OUT = 4,
};
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
enum GESTURE_MODE_TYPE {
	GESTURE_DOUBLETAP,
	GESTURE_AOD,
	GESTURE_FOD,
};
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/\

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
    EVENT_MAP_HOST_CMD                      = 0x50,
    EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
    EVENT_MAP_RESET_COMPLETE                = 0x60,
    EVENT_MAP_FWINFO                        = 0x78,
    EVENT_MAP_PROJECTID                     = 0x9A,
} SPI_EVENT_MAP;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(15*1024)
#define NVT_READ_LEN		(4*1024)
#define NVT_XBUF_LEN		(NVT_TRANSFER_LEN+1+DUMMY_BYTES)

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
extern void set_nvt_gesture_flag(bool en);
/*P16 code for HQFEAT-102772 by xiongdejun at 2025/4/30 start*/
extern void set_nvt_gesture_flag_tianma(bool en);
/*P16 code for HQFEAT-102772 by xiongdejun at 2025/4/30 end*/
extern void set_nvt_gesture_flag_vox(bool en);
extern int update_fod_press_status(int value);
void nvt_ts_fod_down_report(uint16_t fod_x, uint16_t fod_y);
void nvt_ts_fod_up_report(void);
int32_t nvt_set_gesture_switch(uint8_t gesture_switch);
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
int32_t nvt_set_charger_switch(uint8_t charger_switch);
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_fw_crc_enable(void);
void nvt_tx_auto_copy_mode(void);
void nvt_read_fw_history_all(void);
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
void nvt_irq_enable(bool enable);
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
int32_t nvt_set_pocket_palm_switch(uint8_t pocket_palm_switch);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
int32_t nvt_update_firmware(char *firmware_name, uint8_t skip_update_ilm);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_wait_auto_copy(void);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int32_t nvt_read_reg(nvt_ts_reg_t reg, uint8_t *val);
int32_t nvt_write_reg(nvt_ts_reg_t reg, uint8_t val);
#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
int32_t nvt_set_custom_cmd(uint8_t *cmd_buf, uint8_t cmd_len);
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
int32_t nvt_set_edge_reject_switch(uint8_t edge_reject_switch);
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
#endif /* _LINUX_NVT_TOUCH_H */
