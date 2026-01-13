/*
 * Copyright (C) 2024 Novatek, Inc.
 *
 * $Revision: 72896 $
 * $Date: 2020-11-24 17:49:48 +0800 (週二, 24 十一月 2020) $
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
#include <uapi/linux/sched/types.h>
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
#include "nt387xx.h"
#include <linux/power_supply.h>

/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include "../../../gpu/drm/mediatek/mediatek_v2/mi_disp/mi_disp_notifier.h"

#include <drm/drm_panel.h>
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
#include <linux/msm_drm_notify.h>
#elif IS_ENABLED(NVT_FB_NOTIFY)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
#include <linux/earlysuspend.h>
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
#include <linux/soc/qcom/panel_event_notifier.h>
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
#endif

/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 end */
#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 start */
#include <linux/string.h>
#include <linux/hqsysfs.h>
/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 end */
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 start*/
#define NVT_VENDOR_TOUCH_IC '4'
char hex_str[3];
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 end*/
#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
char debug_info_buf[DEBUG_MAX_BUFFER_SIZE] = {0};
EXPORT_SYMBOL(debug_info_buf);
int nvt_read_pos = 0;
int nvt_write_pos = 0;
int is_full = 0;
EXPORT_SYMBOL(nvt_read_pos);
EXPORT_SYMBOL(nvt_write_pos);
EXPORT_SYMBOL(is_full);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
#endif
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
static int nvt_get_charging_status(void);
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
extern void mi_display_gesture_callback_register(void (*cb)(void));
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/

struct nvt_ts_data *ts;
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
uint8_t edge_orientation_store = 1;
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/

/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 start */
uint8_t  tp_fw_version;
static char tp_version_info[128] = "";
/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 end */

#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
extern void Boot_Update_Firmware(struct work_struct *work);
#endif
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
static void nvt_set_gesture_mode(int value);
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 start */
static struct work_struct nvt_touch_resume_work;
static struct workqueue_struct *nvt_touch_resume_workqueue;
static int32_t nvt_ts_resume(struct device *dev);
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
static struct drm_panel *active_panel;
static int nvt_xiaomi_panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 end */
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif IS_ENABLED(NVT_FB_NOTIFY)
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
static void nvt_ts_early_suspend(struct early_suspend *h);
static void nvt_ts_late_resume(struct early_suspend *h);
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
static struct drm_panel *active_panel;
static void *notifier_cookie;
static void nvt_panel_notifier_callback(enum panel_event_notifier_tag tag,
		struct panel_event_notification *event, void *client_data);
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
static int nvt_mtk_drm_notifier_callback(struct notifier_block *nb,
		unsigned long event, void *data);
#endif
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
char *BOOT_UPDATE_FIRMWARE_NAME;
char *MP_UPDATE_FIRMWARE_NAME;
unsigned int g_lcm_panel_id = 0xFF;
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#if NVT_SUPER_RESOLUTION
static int current_super_resolution;
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_POWER,  //GESTURE_WORD_C
	KEY_POWER,  //GESTURE_WORD_W
	KEY_POWER,  //GESTURE_WORD_V
	KEY_WAKEUP,  //GESTURE_DOUBLE_CLICK
	KEY_POWER,  //GESTURE_WORD_Z
	KEY_POWER,  //GESTURE_WORD_M
	KEY_POWER,  //GESTURE_WORD_O
	KEY_POWER,  //GESTURE_WORD_e
	KEY_POWER,  //GESTURE_WORD_S
	KEY_POWER,  //GESTURE_SLIDE_UP
	KEY_POWER,  //GESTURE_SLIDE_DOWN
	KEY_POWER,  //GESTURE_SLIDE_LEFT
	KEY_POWER,  //GESTURE_SLIDE_RIGHT
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 start*/
	KEY_GOTO,  //GESTURE_SINGLE_CLICK
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 end*/
};
#endif

#ifdef CONFIG_MTK_SPI
const struct mt_chip_conf spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#ifdef CONFIG_SPI_MT65XX
const struct mtk_chip_config spi_ctrdata = {
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
};
#endif

static uint8_t bTouchIsAwake = 0;

/*******************************************************
Description:
	Novatek touchscreen irq enable/disable function.

return:
	n.a.
*******************************************************/
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
void nvt_irq_enable(bool enable)
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	NVT_LOG("enable=%d, desc->depth=%d\n", enable, desc->depth);
}

/*******************************************************
Description:
	Novatek touchscreen spi read/write core function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len , NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};

	memset(ts->xbuf, 0, len + DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
		case NVTREAD:
			t.tx_buf = ts->xbuf;
			t.rx_buf = ts->rbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = ts->xbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(client, &m);
}

/*******************************************************
Description:
	Novatek touchscreen spi read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return CTP_SPI_WRITE(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	ret = nvt_set_page(addr);
	if (ret) {
		NVT_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & 0x7F;
	buf[1] = data;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen read value to specific register.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_read_reg(nvt_ts_reg_t reg, uint8_t *val)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = {0};
	uint8_t temp = 0;

	addr = reg.addr;
	mask = reg.mask;
	/* get shift */
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			ret = -1;
			break;
		}
		shift++;
	}
	/* read the byte of the register is in */
	nvt_set_page(addr);
	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_SPI_READ failed!(%d)\n", ret);
		goto nvt_read_register_exit;
	}
	/* get register's value in its field of the byte */
	*val = (buf[1] & mask) >> shift;

nvt_read_register_exit:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen write value to specific register.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_reg(nvt_ts_reg_t reg, uint8_t val)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = {0};
	uint8_t temp = 0;

	addr = reg.addr;
	mask = reg.mask;
	/* get shift */
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			break;
		}
		shift++;
	}
	/* read the byte including this register */
	nvt_set_page(addr);
	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_SPI_READ failed!(%d)\n", ret);
		goto nvt_write_register_exit;
	}
	/* set register's value in its field of the byte */
	temp = buf[1] & (~mask);
	temp |= ((val << shift) & mask);
	/* write back the whole byte including this register */
	buf[0] = addr & 0xFF;
	buf[1] = temp;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_SPI_WRITE failed!(%d)\n", ret);
		goto nvt_write_register_exit;
	}

nvt_write_register_exit:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen clear status & enable fw crc function.

return:
	N/A.
*******************************************************/
void nvt_fw_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 2);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen set boot ready function.

return:
	N/A.
*******************************************************/
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	mdelay(5);
}

/*******************************************************
Description:
	Novatek touchscreen enable auto copy mode function.

return:
	N/A.
*******************************************************/
void nvt_tx_auto_copy_mode(void)
{
	if (ts->auto_copy == CHECK_SPI_DMA_TX_INFO) {
		//---write TX_AUTO_COPY_EN cmds---
		nvt_write_addr(ts->mmap->TX_AUTO_COPY_EN, 0x69);
	} else if (ts->auto_copy == CHECK_TX_AUTO_COPY_EN) {
		//---write SPI_MST_AUTO_COPY cmds---
		nvt_write_addr(ts->mmap->TX_AUTO_COPY_EN, 0x56);
	}

	NVT_ERR("tx auto copy mode %d enable\n", ts->auto_copy);
}

/*******************************************************
Description:
	Novatek touchscreen check spi dma tx info function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_check_spi_dma_tx_info(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 200;

	if (ts->mmap->SPI_DMA_TX_INFO == 0) {
		NVT_ERR("error, SPI_DMA_TX_INFO = 0\n");
		return -1;
	}

	for (i = 0; i < retry; i++) {
		//---set xdata index to SPI_DMA_TX_INFO---
		nvt_set_page(ts->mmap->SPI_DMA_TX_INFO);

		//---read spi dma status---
		buf[0] = ts->mmap->SPI_DMA_TX_INFO & 0x7F;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(1000, 1000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check tx auto copy state function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_check_tx_auto_copy(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 200;

	if (ts->mmap->TX_AUTO_COPY_EN == 0) {
		NVT_ERR("error, TX_AUTO_COPY_EN = 0\n");
		return -1;
	}

	for (i = 0; i < retry; i++) {
		//---set xdata index to SPI_MST_AUTO_COPY---
		nvt_set_page(ts->mmap->TX_AUTO_COPY_EN);

		//---read auto copy status---
		buf[0] = ts->mmap->TX_AUTO_COPY_EN & 0x7F;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(1000, 1000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen wait auto copy finished function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_wait_auto_copy(void)
{
	if (ts->auto_copy == CHECK_SPI_DMA_TX_INFO) {
		return nvt_check_spi_dma_tx_info();
	} else if (ts->auto_copy == CHECK_TX_AUTO_COPY_EN) {
		return nvt_check_tx_auto_copy();
	} else {
		NVT_ERR("failed, not support mode %d!\n", ts->auto_copy);
		return -1;
	}
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_SIF_ADDR---
	nvt_write_addr(ts->swrst_sif_addr, 0xAA);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_SIF_ADDR---
	nvt_write_addr(ts->swrst_sif_addr, 0x69);

	mdelay(5);	//wait tBRST2FR after Bootload RST

	NVT_LOG("end\n");
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	usleep_range(20000, 20000);

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 10000);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen query firmware config.
	function.

return:
	Executive outcomes. 0---success. negative---fail.
*******************************************************/
int32_t nvt_query_fw_config(uint8_t segment, uint8_t offset, uint8_t *configs, uint8_t len)
{
	int32_t ret = 0;
	uint8_t buf[16] = {0};
	int32_t i = 0;
	const int32_t retry = 10;

	if (offset > 7 || len > 8) {
		NVT_ERR("offset=%d, len=%d\n", offset, len);
		ret = -EINVAL;
		goto out;
	}

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x67; /* query cmd */
	buf[2] = 0x00; /* clear handshake fw status */
	buf[3] = 0x01; /* query extend config. */
	buf[4] = segment; /* query which segment */
	CTP_SPI_WRITE(ts->client, buf, 5);

	usleep_range(10000, 10000);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		CTP_SPI_READ(ts->client, buf, 13);

		if (buf[1] == 0xFB)
			break;
		if (buf[2] == 0xA0)
			break;
		usleep_range(1000,1000);
	}

	if (unlikely(i >= retry)) {
		NVT_ERR("query failed, segment=%d, buf[1]=0x%02X, buf[2]=0x%02X\n",
				segment, buf[1], buf[2]);
		ret = -1;
	} else if (buf[1] == 0xFB) {
		NVT_ERR("query cmd / extend config / segment not support by this FW, segment=%d\n",
				segment);
		ret = -1;
	} else {
		memcpy(configs, buf + 5 + offset, len);
		for (i = 0; i < len; i++)
			printk("0x%02X, ", *(configs + i));
		printk("\n");
		ret = 0;
	}

out:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;
	uint8_t configs[8] = {0};

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_SPI_READ(ts->client, buf, 39);
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->fw_ver = 0;
			NVT_ERR("Set default fw_ver=%d\n", ts->fw_ver);
			ret = -1;
			goto out;
		}
	}
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->query_config_ver = (uint16_t)((buf[20] << 8) | buf[19]);
	ts->nvt_pid = (uint16_t)((buf[36] << 8) | buf[35]);

	NVT_LOG("fw_ver=0x%02X, fw_type=0x%02X, PID=0x%04X\n", ts->fw_ver, buf[14], ts->nvt_pid);
/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 start */
	tp_fw_version = ts->fw_ver;
	NVT_LOG("tp_fw_version = 0x%02x \n", tp_fw_version);
/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 end */
	if (ts->pen_support) {
		NVT_LOG("query config version: 0x%04X\n", ts->query_config_ver);
		if (ts->query_config_ver == 0x01) {
			if (nvt_query_fw_config(1, 0, configs, 8)) {
				ret = -1;
				goto out;
			}
			ts->pen_x_num_x = configs[0];
			ts->pen_x_num_y = configs[1];
			ts->pen_y_num_x = configs[2];
			ts->pen_y_num_y = configs[3];
		}
		NVT_LOG("pen_x_num_x=%d, pen_x_num_y=%d\n", ts->pen_x_num_x, ts->pen_x_num_y);
		NVT_LOG("pen_y_num_x=%d, pen_y_num_y=%d\n", ts->pen_y_num_x, ts->pen_y_num_y);
	}

	ret = 0;
out:

	return ret;
}

/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 start */
void get_tp_info(void)
{
	nvt_get_fw_info();
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
	if (g_lcm_panel_id == TOUCH_SELECT_0A_CSOT) {
		sprintf(tp_version_info, "[Vendor]:Huaxing CSOT [TP-IC]:NT38771 [FW]:0x%x\n", tp_fw_version);
	} else if (g_lcm_panel_id == TOUCH_SELECT_0B_TIANMA) {
		sprintf(tp_version_info, "[Vendor]:TianMa [TP-IC]:NT38771 [FW]:0x%x\n", tp_fw_version);
	} else if (g_lcm_panel_id == TOUCH_SELECT_0C_VISIONOX) {
		sprintf(tp_version_info, "[Vendor]:Visionox [TP-IC]:NT38771 [FW]:0x%x\n", tp_fw_version);
	} else {
		NVT_ERR("TP is not CORRECT!");
	}
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
	NVT_LOG("hw_info:tp_version %s\n", tp_version_info);
	hq_regiser_hw_info(HWID_CTP, tp_version_info);
}
/* P16 code for HQFEAT-89651 by liaoxianguo at 2025/3/24 end */
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 start*/
static u8 nvt_panel_vendor_read(void)
{
    if (ts)
        return ts->lockdown[0];
    else
        return 0;
}
static u8 nvt_panel_color_read(void)
{
    if (ts)
        return ts->lockdown[2];
    else
        return 0;
}
static u8 nvt_panel_display_read(void)
{
    if (ts)
        return ts->lockdown[1];
    else
        return 0;
}
static char nvt_touch_vendor_read(void)
{
    return NVT_VENDOR_TOUCH_IC;
}
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 end*/
/*******************************************************
Description:
	Novatek touchscreen set customized command function.

return:
	Executive outcomes. 0---success. -5---fail.
*******************************************************/
int32_t nvt_set_custom_cmd(uint8_t *cmd_buf, uint8_t cmd_len)
{
	int32_t ret = 0;
	int32_t i;
	const int32_t retry = 5;
	uint8_t buf[16] = {0};
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	if(ts->nvt_tool_in_use){
		NVT_ERR("NVT tool in use.\n");
		return -EBUSY;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/

	if (((cmd_len + 1) > sizeof(buf)) || (cmd_len == 0)) {
		NVT_ERR("cmd_len %d error, buffer size %ld\n", cmd_len, sizeof(buf));
		ret = -EINVAL;
		goto out;
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 start */
	if (cmd_len == 1)
		NVT_LOG("command = 0x%02X\n", cmd_buf[0]);
	else
		NVT_LOG("command = 0x%02X 0x%02X\n", cmd_buf[0], cmd_buf[1]);
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/3/27 end */
	for (i = 0; i < retry; i++) {
		if (buf[1] != cmd_buf[0]) {
			//---set cmd---
			buf[0] = EVENT_MAP_HOST_CMD;
			memcpy(buf + 1, cmd_buf, cmd_len);
			CTP_SPI_WRITE(ts->client, buf, cmd_len + 1);
		}

		usleep_range(20000, 20000);

		//---read cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(i >= retry)) {
		NVT_ERR("send cmd 0x%02X failed, buf[1]=0x%02X\n", cmd_buf[0], buf[1]);
		/*P16 code for BUGP16-2768 by liuyupei at 2025/5/16 start*/
		nvt_read_fw_history_all();
		/*P16 code for BUGP16-2768 by liuyupei at 2025/5/16 end*/
		ret = -EIO;
	} else {
		NVT_LOG("send cmd 0x%02X success, tried %d times\n", cmd_buf[0], i);
		ret = 0;
	}

out:
	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		NVT_ERR("invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
    kfree(buf);
kzalloc_failed:
	return ret;
}

static struct attribute *nvt_ts_attributes[] = {
	NULL
};

static struct attribute_group nvt_ts_attribute_group = {
	.attrs = nvt_ts_attributes,
};

#define NVT_TOUCH_SYSFS_LINK "nvt_touch"
static int32_t nvt_touch_sysfs_init(void)
{
	int32_t ret = 0;

	ret = sysfs_create_link(ts->input_dev->dev.kobj.parent, &ts->input_dev->dev.kobj, NVT_TOUCH_SYSFS_LINK);
	if (ret != 0) {
		NVT_ERR("sysfs create link %s failed. ret=%d", NVT_TOUCH_SYSFS_LINK, ret);
		goto exit_nvt_touch_sysfs_init;
	}

	ret = sysfs_create_group(&ts->input_dev->dev.kobj, &nvt_ts_attribute_group);
	if (ret != 0) {
		NVT_ERR("sysfs create group failed. ret=%d\n", ret);
		goto exit_nvt_touch_sysfs_init;
	}

exit_nvt_touch_sysfs_init:
	return ret;
}

static void nvt_touch_sysfs_deinit(void)
{
	sysfs_remove_group(&ts->input_dev->dev.kobj, &nvt_ts_attribute_group);

	sysfs_remove_link(ts->input_dev->dev.kobj.parent, NVT_TOUCH_SYSFS_LINK);
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	ts->nvt_tool_in_use = true;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	ts->nvt_tool_in_use = false;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	return 0;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};
#else
static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("Succeeded!\n");
	}

	NVT_LOG("============================================================\n");
	NVT_LOG("Create /proc/%s\n", DEVICE_NAME);
	NVT_LOG("============================================================\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", DEVICE_NAME);
	}
}
#endif

#if WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
#define GESTURE_SINGLE_CLICK    25
#define GESTURE_FOD             30
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

#define FOD_DOWN 1
#define FOD_UP 2

/*******************************************************
Description:
	Novatek touchscreen wake up gesture key report function.

return:
	n.a.
*******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	uint8_t fod_status = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n", gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
		case GESTURE_WORD_C:
			NVT_LOG("Gesture : Word-C.\n");
			keycode = gesture_key_array[0];
			break;
		case GESTURE_WORD_W:
			NVT_LOG("Gesture : Word-W.\n");
			keycode = gesture_key_array[1];
			break;
		case GESTURE_WORD_V:
			NVT_LOG("Gesture : Word-V.\n");
			keycode = gesture_key_array[2];
			break;
		case GESTURE_DOUBLE_CLICK:
		/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 start*/
			if (ts->gesture_command & 0x01) {
				/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 start*/
				if (ts->nonui_status){
					NVT_LOG("nonui forbide double click");
					return;
				}
					NVT_LOG("Gesture : Double Click.\n");
					keycode = gesture_key_array[3];
				/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 end*/
			} else {
				NVT_LOG("Gesture : Double Click Not Enable.\n");
				keycode = 0;
			}
		/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 end*/
			break;
		case GESTURE_WORD_Z:
			NVT_LOG("Gesture : Word-Z.\n");
			keycode = gesture_key_array[4];
			break;
		case GESTURE_WORD_M:
			NVT_LOG("Gesture : Word-M.\n");
			keycode = gesture_key_array[5];
			break;
		case GESTURE_WORD_O:
			NVT_LOG("Gesture : Word-O.\n");
			keycode = gesture_key_array[6];
			break;
		case GESTURE_WORD_e:
			NVT_LOG("Gesture : Word-e.\n");
			keycode = gesture_key_array[7];
			break;
		case GESTURE_WORD_S:
			NVT_LOG("Gesture : Word-S.\n");
			keycode = gesture_key_array[8];
			break;
		case GESTURE_SLIDE_UP:
			NVT_LOG("Gesture : Slide UP.\n");
			keycode = gesture_key_array[9];
			break;
		case GESTURE_SLIDE_DOWN:
			NVT_LOG("Gesture : Slide DOWN.\n");
			keycode = gesture_key_array[10];
			break;
		case GESTURE_SLIDE_LEFT:
			NVT_LOG("Gesture : Slide LEFT.\n");
			keycode = gesture_key_array[11];
			break;
		case GESTURE_SLIDE_RIGHT:
			NVT_LOG("Gesture : Slide RIGHT.\n");
			keycode = gesture_key_array[12];
			break;
		case GESTURE_SINGLE_CLICK:
			/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 start*/
			if (ts->gesture_command & 0x02) {
				if (ts->nonui_status){
					NVT_LOG("nonui forbide single click");
					return;
				}
				NVT_LOG("Gesture : Single Click.\n");
				keycode = gesture_key_array[13];
			} else {
				NVT_LOG("Gesture : Single Click Not Enable.\n");
				keycode = 0;
			}
			break;
		case GESTURE_FOD:
			if (ts->nonui_status == 2){
				NVT_LOG("nonui forbide fod down");
				return;
			} else {
			/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 end*/
			/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
			fod_status = data[4];
			input_x = (uint16_t)((data[5] << 8) | data[6]);
			input_y = (uint16_t)((data[7] << 8) | data[8]);
			if (fod_status == FOD_DOWN || fod_status == FOD_UP) {
				/*P16 code for BUGP16-2768 by liuyupei at 2025/5/16 start*/
				NVT_LOG("get FOD event, fod_status=%d ,input_x=%d, input_y=%d\n", fod_status,input_x, input_y);
				/*P16 code for BUGP16-2768 by liuyupei at 2025/5/16 end*/
				// report FOD event.
				if (ts->gesture_command & 0x04) {
						if (fod_status == FOD_DOWN) {
							if(!ts->fod_finger) {
								NVT_LOG("Gesture : FOD Down, input_x=%d, input_y=%d.\n", input_x, input_y);
							}
							nvt_ts_fod_down_report(input_x, input_y);
						} else if (fod_status == FOD_UP) {
							if (ts->fod_finger) {
								NVT_LOG("Gesture : FOD Up, fod_id:%d\n", TOUCH_FOD_ID);
								nvt_ts_fod_up_report();
							}
						}
				} else {
					NVT_LOG("Gesture : FOD Not Enable.\n");
				}
			}
			/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
			break;
			}
		default:
			break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
void nvt_ts_fod_down_report(uint16_t fod_x, uint16_t fod_y)
{
	update_fod_press_status(1);
	ts->fod_finger = true;
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 start*/
	ts->fod_rpt_slot_9 = true;
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 end*/
	/*report input event*/
	input_mt_slot(ts->input_dev, TOUCH_FOD_ID);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
	input_report_key(ts->input_dev, BTN_INFO, 1);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, fod_x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, fod_y);
	input_sync(ts->input_dev);
}
void nvt_ts_fod_up_report(void)
{
	input_mt_slot(ts->input_dev, TOUCH_FOD_ID);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_report_key(ts->input_dev, BTN_INFO, 0);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
	input_sync(ts->input_dev);
	update_fod_press_status(0);
	ts->fod_finger = false;
}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/

/*******************************************************
Description:
	Novatek touchscreen parse device tree function.

return:
	n.a.
*******************************************************/
#ifdef CONFIG_OF
static int32_t nvt_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio(np, "novatek,reset-gpio", 0);
	NVT_LOG("novatek,reset-gpio=%d\n", ts->reset_gpio);
#endif
	ts->irq_gpio = of_get_named_gpio(np, "novatek,irq-gpio", 0);
	NVT_LOG("novatek,irq-gpio=%d\n", ts->irq_gpio);

	ts->pen_support = of_property_read_bool(np, "novatek,pen-support");
	NVT_LOG("novatek,pen-support=%d\n", ts->pen_support);
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 start*/
	ret = of_property_read_u32_array(np, "novatek,touch-game-param-config1", ts->gamemode_config[0], 4);
	if (ret) {
		NVT_ERR("Failed to get touch-game-param-config1\n");
		ret = 0;
	} else {
		NVT_LOG("read touch gamemode parameter config1:[%d, %d, %d, %d]",
		ts->gamemode_config[0][0], ts->gamemode_config[0][1],
		ts->gamemode_config[0][2], ts->gamemode_config[0][3]);
	}
	ret = of_property_read_u32_array(np, "novatek,touch-game-param-config2", ts->gamemode_config[1], 4);
	if (ret) {
		NVT_ERR("Failed to get touch-game-param-config2\n");
		ret = 0;
	} else {
		NVT_LOG("read touch gamemode parameter config2:[%d, %d, %d, %d]",
		ts->gamemode_config[1][0], ts->gamemode_config[1][1],
		ts->gamemode_config[1][2], ts->gamemode_config[1][3]);
	}
	ret = of_property_read_u32_array(np, "novatek,touch-game-param-config3", ts->gamemode_config[2], 4);
	if (ret) {
		NVT_ERR("Failed to get touch-game-param-config3\n");
		ret = 0;
	} else {
		NVT_LOG("read touch gamemode parameter config3:[%d, %d, %d, %d]",
		ts->gamemode_config[2][0], ts->gamemode_config[2][1],
		ts->gamemode_config[2][2], ts->gamemode_config[2][3]);
	}
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 end*/
	return ret;
}
#else
static int32_t nvt_parse_dt(struct device *dev)
{
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	ts->irq_gpio = NVTTOUCH_INT_PIN;
	ts->pen_support = false;
	return 0;
}
#endif

/*******************************************************
Description:
	Novatek touchscreen config and request gpio

return:
	Executive outcomes. 0---succeed. not 0---failed.
*******************************************************/
static int nvt_gpio_config(struct nvt_ts_data *ts)
{
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_HIGH, "NVT-tp-rst");
		if (ret) {
			NVT_ERR("Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(ts->irq_gpio)) {
		ret = gpio_request_one(ts->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			NVT_ERR("Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen deconfig gpio

return:
	n.a.
*******************************************************/
static void nvt_gpio_deconfig(struct nvt_ts_data *ts)
{
	if (gpio_is_valid(ts->irq_gpio))
		gpio_free(ts->irq_gpio);
#if NVT_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//NVT_ERR("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, esd_retry);
		/* do esd recovery, reload fw */
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
		/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
		nvt_fw_reload_recovery();
		/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#define PEN_DATA_LEN 14
#if CHECK_PEN_DATA_CHECKSUM
static int32_t nvt_ts_pen_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Calculate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length - 1]) {
		NVT_ERR("pen packet checksum not match. (buf[%d]=0x%02X, checksum=0x%02X)\n",
			length - 1, buf[length - 1], checksum);
		//--- dump pen buf ---
		for (i = 0; i < length; i++) {
			printk("%02X ", buf[i]);
		}
		printk("\n");

		return -1;
	}

	return 0;
}
#endif // #if CHECK_PEN_DATA_CHECKSUM

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt = 0;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
   uint32_t recovery_cnt_max = 10;
   uint8_t recovery_enable = false;
   uint8_t i = 0;

   recovery_cnt++;

   /* check pattern */
   for (i=1 ; i<7 ; i++) {
       if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
           recovery_cnt = 0;
           break;
       }
   }

   if (recovery_cnt > recovery_cnt_max){
       recovery_enable = true;
       recovery_cnt = 0;
   }

   return recovery_enable;
}

void nvt_clear_aci_error_flag(void)
{
	if (ts->mmap->ACI_ERR_CLR_ADDR == 0)
		return;

	nvt_write_addr(ts->mmap->ACI_ERR_CLR_ADDR, 0xA5);

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}
#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

void nvt_read_fw_history(uint32_t fw_history_addr)
{
	uint8_t i = 0;
	uint8_t buf[65];
	char str[128];

	if (fw_history_addr == 0)
		return;

	nvt_set_page(fw_history_addr);

	buf[0] = (uint8_t) (fw_history_addr & 0x7F);
	CTP_SPI_READ(ts->client, buf, 64+1);	//read 64bytes history

	//print all data
	NVT_LOG("fw history 0x%X: \n", fw_history_addr);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str),
				"%02X %02X %02X %02X %02X %02X %02X %02X  "
				"%02X %02X %02X %02X %02X %02X %02X %02X\n",
				buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
				buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
				buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
				buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
		NVT_LOG("%s", str);
	}

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

void nvt_read_fw_history_all(void) {

	/* ICM History */
	nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
	nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);

	/* ICS History */
	if (ts->is_cascade) {
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0_ICS);
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1_ICS);
	}
}

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
static int nvt_enable_gesture_mode(int value)
{
	int32_t ret = 0;
	uint8_t doubletap_enable = 0;
	uint8_t singletap_enable = 0;
	uint8_t fod_enable = 0;
	uint8_t buf[4] = {0};

	if (!ts) {
		NVT_ERR("Driver data is Null");
		return -ENOMEM;
	}

	// set gesture enable/disable
	nvt_set_gesture_switch((uint8_t)(ts->gesture_command & 0xFF));

	msleep(35);
	if (value) {
		/*---write command to enter "wakeup gesture mode"---*/
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
		ret = CTP_SPI_WRITE(ts->client, buf, 2);
		if (ret < 0) {
			NVT_ERR("set cmd failed!\n");
		}
		doubletap_enable = ts->gesture_command & 0x01;
		singletap_enable = ts->gesture_command & 0x02;
		fod_enable = ts->gesture_command & 0x04;
		NVT_LOG("Gesture mode on, %s singletap gesture, %s doubletap gesture, %s fod\n",
				singletap_enable ? "Enable" : "Disable", doubletap_enable ? "Enable" : "Disable", fod_enable ? "Enable" : "Disable");
	} else {
		/*---write command to enter "deep sleep mode"---*/
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x11;
		ret = CTP_SPI_WRITE(ts->client, buf, 2);
		if (ret < 0) {
			NVT_ERR("set cmd failed!\n");
		}
		NVT_LOG("Enter deep sleep mode\n");
	}

	return ret;
}

static void nvt_fw_reload_recovery(void)
{
	int i = 0;

	NVT_LOG("++\n");

	/* release all touches */
	if(ts && ts->input_dev) {
#if MT_PROTOCOL_B
		for (i = 0; i < ts->max_touch_num; i++) {
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			//input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#if NVT_DRIVER_INSERT_FRAME
			ts->input_event_state[i] = 0;
#endif
		}
#endif
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
		input_mt_sync(ts->input_dev);
#endif
		input_sync(ts->input_dev);
	}

	/* reload gesture cmd when open gesture */
	if (!(!ts) && (!bTouchIsAwake) && (ts->gesture_command)) {
		nvt_enable_gesture_mode(true);
	}
	/*P16 code for BUGP16-8567 by liuyupei at 2025/7/23 start*/
	if (bTouchIsAwake) {
		nvt_set_gesture_switch(0x00);
	}
	/*P16 code for BUGP16-8567 by liuyupei at 2025/7/23 end*/

	NVT_LOG("--\n");
}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/

#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i + 1];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		NVT_ERR("i2c/spi packet checksum not match. (point_data[%d]=0x%02X, checksum=0x%02X)\n",
				length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			NVT_LOG("%02X %02X %02X %02X %02X %02X\n",
					buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		NVT_LOG("%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}
#endif /* POINT_DATA_CHECKSUM */

/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
int32_t nvt_check_palm(uint8_t input_id, uint8_t *data)
{
	int32_t ret = 0;
	uint8_t func_type = data[2];
	uint8_t palm_state = data[3];

	if ((input_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_PALM)) {
		ret = palm_state;
		if (palm_state == POCKET_PALM_ON) {
			NVT_LOG("get pocket palm on event.\n");
			update_palm_sensor_value(1);
		} else if (palm_state == POCKET_PALM_OFF) {
			NVT_LOG("get pocket palm off event.\n");
			update_palm_sensor_value(0);
		} else {
			// should never go here
			NVT_ERR("invalid palm state %d!\n", palm_state);
			ret = -1;
		}
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	} else {
		// not palm event
		ret = 0;
	}

	return ret;
}

static int nvt_palm_sensor_write(int value)
{
	int ret = 0;
	NVT_LOG("enter %d %d\n", value, ts->palm_sensor_switch);
	ts->palm_sensor_switch = value;
	if (ts->dev_pm_suspend) {
		NVT_LOG("tp has dev_pm_suspend status\n");
		return 0;
	}
	if (bTouchIsAwake) {
		mutex_lock(&ts->lock);
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(false);
#endif
		ret = nvt_set_pocket_palm_switch(value);
		mutex_unlock(&ts->lock);
	}
	return ret;
}
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
static void write_to_buffer(char *data, int len){
	int first_part = 0;
	int new_write_pos = 0;
	if(len > DEBUG_MAX_BUFFER_SIZE || len <= 0){
		return;
	}
	if(!data){
		return;
        }
	if(nvt_write_pos > DEBUG_MAX_BUFFER_SIZE){
        	return;
        }
	int space_len_to_end = DEBUG_MAX_BUFFER_SIZE - nvt_write_pos;//rest space
	if(len <= space_len_to_end){
		first_part = len;
	}else{
		first_part = space_len_to_end;
	}
	NVT_LOG("first_part = %d\n", first_part);
	memcpy(debug_info_buf + nvt_write_pos ,data ,first_part);

	int second_part = len - first_part;
	if(second_part > 0){
        	memcpy(debug_info_buf ,data + first_part ,second_part);
	}
	new_write_pos = (nvt_write_pos + len)%DEBUG_MAX_BUFFER_SIZE;
	if(!is_full){
		if(second_part > 0 && new_write_pos > nvt_read_pos){
			nvt_read_pos = new_write_pos;
			is_full = 1;
		}else if(new_write_pos == nvt_read_pos){
			is_full = 1;
		}
	}else{
		nvt_read_pos = (nvt_read_pos + len)%DEBUG_MAX_BUFFER_SIZE;
	}
	nvt_write_pos = new_write_pos;
	NVT_LOG("write_pos = %d\n", nvt_write_pos);
}
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
static void nvt_fw_debug_info_print(uint8_t *data, uint32_t data_len)
{
	int x_num;
	int y_num;
	int i, j;
	char *tmp_log = NULL;
	int data_char_num;
	int tmp_log_len;

	x_num = 32;
	y_num = data_len / x_num;
	data_char_num = 5;

	tmp_log = (char *)kzalloc(x_num * data_char_num + 1, GFP_KERNEL);
	if (!tmp_log) {
		NVT_ERR("kzalloc for tmp_log failed!\n ");
		return;
	}
	write_to_buffer("debug info:" , 11);
	write_to_buffer("\n" , 1);
	for (j = 0; j < y_num; j++) {
		for (i = 0; i < x_num; i++) {
			sprintf(tmp_log + i * data_char_num, " %3d,", data[j * x_num + i]);
		}
		tmp_log[x_num * data_char_num] = '\0';
		NVT_LOG("%s", tmp_log);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
		tmp_log_len = strlen(tmp_log);
        	NVT_LOG("write_pos = %d\n", nvt_write_pos);
		write_to_buffer(tmp_log ,tmp_log_len);
		write_to_buffer("\n",1);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
		memset(tmp_log, 0, x_num * data_char_num + 1);
		printk("\n");
	}
	if (data_len % x_num) {
		for (i = 0; i < (data_len % x_num); i++) {
			sprintf(tmp_log + i * data_char_num, " %03d,", data[y_num * x_num + i]);
		}
		tmp_log[(data_len % x_num) * data_char_num] = '\0';
		NVT_LOG("%s", tmp_log);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
        	tmp_log_len = strlen(tmp_log);
		write_to_buffer(tmp_log ,tmp_log_len);
		write_to_buffer("\n",1);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
		memset(tmp_log, 0, x_num * data_char_num + 1);
		printk("\n");
	}

	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
#if NVT_DRIVER_INSERT_FRAME
// =========== hrtimer callback ==============
enum hrtimer_restart nvt_hrtimer_callback(struct hrtimer *timer)
{
	int32_t i = 0;
	int32_t finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		if (ts->input_event_state[i] == 1) {
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
			//input_report_abs(ts->input_dev, ABS_MT_PRESSURE, TOUCH_FORCE_NUM);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->pre_fw_input_x[i]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->pre_fw_input_y[i]);
			//printk("NVT FW x=%d,y=%d\n",  ts->pre_fw_input_x[i] ,  ts->pre_fw_input_y[i]);
			finger_cnt++;
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
	input_sync(ts->input_dev);

	return HRTIMER_NORESTART;
}
#endif /* NVT_DRIVER_INSERT_FRAME */
#if NVT_SUPER_RESOLUTION
#define POINT_DATA_LEN 108
#else
#define POINT_DATA_LEN 65
#endif

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#define FINGER_FOD_DOWN 0x03
#define FINGER_FOD_UP 0x04
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*******************************************************
Description:
	Novatek touchscreen work function.

return:
	n.a.
*******************************************************/
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_FW_DEBUG_INFO_LEN + 1 + DUMMY_BYTES] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
	uint8_t input_status = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */
	int32_t i = 0;
  	int32_t j = 0;
	int32_t finger_cnt = 0;
	uint8_t pen_format_id = 0;
	uint32_t pen_x = 0;
	uint32_t pen_y = 0;
	uint32_t pen_pressure = 0;
	uint32_t pen_distance = 0;
	int8_t pen_tilt_x = 0;
	int8_t pen_tilt_y = 0;
	uint32_t pen_btn1 = 0;
	uint32_t pen_btn2 = 0;
	uint32_t pen_battery = 0;
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#if NVT_SUPER_RESOLUTION
	int super_resolution_factor = SUPER_RESOLUTION_FACOTR / current_super_resolution;
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	static struct task_struct *touch_task = NULL;
	struct sched_param par = { .sched_priority = MAX_RT_PRIO - 1 };
	uint8_t *fw_debug_info;
#if NVT_DRIVER_INSERT_FRAME
	uint32_t driver_input_x = 0;
	uint32_t driver_input_y = 0;
#endif
	if (touch_task == NULL) {
		/* touch priority improve */
		touch_task = current;
		printk("nvt-ts: touch_irq_thread prio improve to %d", MAX_RT_PRIO - 1);
		sched_setscheduler_nocheck(touch_task, SCHED_FIFO, &par);
	}
	cpu_latency_qos_add_request(&ts->pm_qos_req_irq, 0);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		pm_wakeup_event(&ts->client->dev, 5000);
	}
#endif

	mutex_lock(&ts->lock);

#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
	if (ts->dev_pm_suspend) {
		ret = wait_for_completion_timeout(&ts->dev_pm_resume_completion, msecs_to_jiffies(500));
		if (!ret) {
			NVT_ERR("system(bus) can't finished resuming procedure, skip it!\n");
			goto XFER_ERROR;
		}
	}
#endif /* NVT_PM_WAIT_BUS_RESUME_COMPLETE */
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
	if (ts->fw_debug_info_switch) {
		ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_FW_DEBUG_INFO_LEN + 1);
		fw_debug_info = point_data + 1 + FW_DEBUG_INFO_OFFSET;
		nvt_fw_debug_info_print(fw_debug_info, FW_DEBUG_INFO_LEN);
		// ToDo: memcpy(fw_debug_info_buf, fw_debug_info, FW_DEBUG_INFO_LEN);
	} else {
#if NVT_SUPER_RESOLUTION
	ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
#else /* #if NVT_SUPER_RESOLUTION */
	if (ts->pen_support)
		ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + PEN_DATA_LEN + 1);
	else
		ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
#endif /* #if NVT_SUPER_RESOLUTION */
        }
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
	if (ret < 0) {
		NVT_ERR("CTP_SPI_READ failed.(%d)\n", ret);
		goto XFER_ERROR;
	}
/*
	//--- dump SPI buf ---
	for (i = 0; i < 10; i++) {
		printk("%02X %02X %02X %02X %02X %02X  ",
			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
	printk("\n");
*/

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		NVT_ERR("Recover for fw reset, %02X\n", point_data[1]);
		if (point_data[1] == 0xFE) {
			nvt_sw_reset_idle();
			nvt_clear_aci_error_flag();
		}
		nvt_read_fw_history_all();
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false);
		nvt_read_fw_history_all();
		nvt_fw_reload_recovery();
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
    if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
        ret = nvt_ts_point_data_checksum(point_data, POINT_DATA_CHECKSUM_LEN);
        if (ret) {
            goto XFER_ERROR;
        }
    }
#endif /* POINT_DATA_CHECKSUM */
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
input_id = (uint8_t)(point_data[1] >> 3);

	if (nvt_check_palm(input_id, point_data)) {
		goto XFER_ERROR; // to skip point data parsing
	}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/4/13 start*/
#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(input_id, point_data);
		goto XFER_ERROR;
	}
#endif
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
	finger_cnt = 0;

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num))
			continue;

		input_status = point_data[position] & 0x07;
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
		if ((input_status == 0x01) || (input_status == 0x02) || (input_status == FINGER_FOD_DOWN) || (input_status == FINGER_FOD_UP)) {	//finger down (enter & moving)
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
#if NVT_SUPER_RESOLUTION
			input_x = (uint32_t)(point_data[position + 1] << 8) + (uint32_t) (point_data[position + 2]);
			input_y = (uint32_t)(point_data[position + 3] << 8) + (uint32_t) (point_data[position + 4]);
			if ((input_x > (TOUCH_MAX_WIDTH * SUPER_RESOLUTION_FACOTR - 1)) || (input_y > (TOUCH_MAX_HEIGHT * SUPER_RESOLUTION_FACOTR - 1)))
				continue;
			input_w = (uint32_t)(point_data[position + 5]);
			if (input_w == 0)
				input_w = 1;
			input_p = (uint32_t)(point_data[1 + 98 + i]);
			if (input_p == 0)
				input_p = 1;
#else /* #if NVT_SUPER_RESOLUTION */
			input_x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t) (point_data[position + 3] >> 4);
			input_y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t) (point_data[position + 3] & 0x0F);
			if ((input_x > (TOUCH_MAX_WIDTH - 1)) || (input_y > (TOUCH_MAX_HEIGHT - 1)))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]) + (uint32_t)(point_data[i + 63] << 8);
				if (input_p > TOUCH_FORCE_NUM)
					input_p = TOUCH_FORCE_NUM;
			} else {
				input_p = (uint32_t)(point_data[position + 5]);
			}
			if (input_p == 0)
				input_p = 1;
#endif /* #if NVT_SUPER_RESOLUTION */

#if MT_PROTOCOL_B
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 start*/
			if (ts->fod_rpt_slot_9) { /* i.e. situation that FOD unlock from suspend */
				/* skip other non-FOD fingers and only report FOD finger in this situation */
				if ((input_status == FINGER_FOD_DOWN) || (input_status == FINGER_FOD_UP)) {
                                  	ts->fod_input_id = input_id;
					i = ts->max_touch_num;
					for (j = 0; j < TOUCH_FOD_ID; j++) {
						press_id[j] = 0;
					}
					press_id[TOUCH_FOD_ID] = 1;
					input_mt_slot(ts->input_dev, TOUCH_FOD_ID);			
				} else {
                                  	if(input_id == ts->fod_input_id){
                                          	i = ts->max_touch_num;
                                          	for (j = 0; j < TOUCH_FOD_ID; j++) {
							press_id[j] = 0;
						}
                                          	press_id[TOUCH_FOD_ID] = 1;
						input_mt_slot(ts->input_dev, TOUCH_FOD_ID);
                                        }else{
						/* skip other non-FOD fingers */
						continue;
                                        }
				}
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 end*/
			} else {
				press_id[input_id - 1] = 1;
				input_mt_slot(ts->input_dev, input_id - 1);
			}
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, input_id - 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */
#if NVT_DRIVER_INSERT_FRAME
			if (current_super_resolution == 16) { // i.e. in game mode
				if(ts->input_event_state[input_id - 1]  == 1) {
					driver_input_x = ts->pre_fw_input_x[input_id - 1] + (int32_t)(input_x - ts->pre_fw_input_x[input_id - 1])/2;
					driver_input_y = ts->pre_fw_input_y[input_id - 1] + (int32_t)(input_y - ts->pre_fw_input_y[input_id - 1])/2;
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, driver_input_x);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, driver_input_y);
					ts->pre_fw_input_x[input_id - 1] = input_x;
					ts->pre_fw_input_y[input_id - 1] = input_y;
					//printk("NVT Driver  x=%d,y=%d,pre x=%d,pre y=%d\n",  driver_input_x , driver_input_y, ts->pre_fw_input_x[input_id - 1], ts->pre_fw_input_y[input_id - 1]);
				} else { //First touch event.
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
					ts->pre_fw_input_x[input_id - 1] = input_x;
					ts->pre_fw_input_y[input_id - 1] = input_y;
					//printk("NVT FW  x=%d,y=%d\n",input_x , input_y);
				}
				ts->input_event_state[input_id - 1] = 1;  //1:move
			} else { // not in game mode
#if NVT_SUPER_RESOLUTION
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, (int)((int)(input_x / super_resolution_factor) * super_resolution_factor));
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (int)((int)(input_y / super_resolution_factor) * super_resolution_factor));
#else
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
#endif
			} /* if (current_super_resolution == 16) */
#else /* NVT_DRIVER_INSERT_FRAME */
#if NVT_SUPER_RESOLUTION
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, (int)((int)(input_x / super_resolution_factor) * super_resolution_factor));
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, (int)((int)(input_y / super_resolution_factor) * super_resolution_factor));
#else
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
#endif
#endif /* NVT_DRIVER_INSERT_FRAME */
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
			//input_report_abs(ts->input_dev, ABS_MT_PRESSURE, input_p);
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

			finger_cnt++;
			if (input_status == FINGER_FOD_DOWN) {
				if (ts->gesture_command & 0x04) { // if fod is enabled
					if(!ts->fod_finger) {
					NVT_LOG("FOD Down, input_x=%d, input_y=%d\n", input_x, input_y);
					}
					update_fod_press_status(1);
					ts->fod_finger = true;
					input_report_key(ts->input_dev, BTN_INFO, 1);
				}
			} else if (input_status == FINGER_FOD_UP) {
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 start*/
				if (ts->fod_finger) {
					NVT_LOG("FOD Up\n");
					update_fod_press_status(0);
					input_report_key(ts->input_dev, BTN_INFO, 0);
					ts->fod_finger = false;
				}
			}
		}
	}

	if (finger_cnt == 0) {
		if (ts->fod_finger) {
			NVT_LOG("FOD report Up\n");
			update_fod_press_status(0);
			input_report_key(ts->input_dev, BTN_INFO, 0);
			ts->fod_finger = false;// ToDo: report to upper layer
		}
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 start*/
	ts->fod_rpt_slot_9 = false;
	ts->fod_input_id = 0;
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 end*/
/*P16 code for BUGP16-6740 by liuyupei at 2025/7/1 end*/
	}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/4/13 end*/
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		if (press_id[i] != 1) {
			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			//input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
#if NVT_DRIVER_INSERT_FRAME
			ts->input_event_state[i] = 0;
#endif
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* MT_PROTOCOL_B */

	input_sync(ts->input_dev);
#if NVT_DRIVER_INSERT_FRAME
	if (current_super_resolution == 16) { // i.e. insert frame in game mode
		//start timer and run callback function after 2.083ms
		hrtimer_start(&ts->nvt_hrtimer, ts->kt_delay, HRTIMER_MODE_REL);
	}
#endif

	if (ts->pen_support) {
/*
		//--- dump pen buf ---
		printk("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			point_data[66], point_data[67], point_data[68], point_data[69], point_data[70],
			point_data[71], point_data[72], point_data[73], point_data[74], point_data[75],
			point_data[76], point_data[77], point_data[78], point_data[79]);
*/
#if CHECK_PEN_DATA_CHECKSUM
		if (nvt_ts_pen_data_checksum(&point_data[66], PEN_DATA_LEN)) {
			// pen data packet checksum not match, skip it
			goto XFER_ERROR;
		}
#endif // #if CHECK_PEN_DATA_CHECKSUM

		// parse and handle pen report
		pen_format_id = point_data[66];
		if (pen_format_id != 0xFF) {
			if (pen_format_id == 0x01) {
				// report pen data
				pen_x = (uint32_t)(point_data[67] << 8) + (uint32_t)(point_data[68]);
				pen_y = (uint32_t)(point_data[69] << 8) + (uint32_t)(point_data[70]);
				if ((pen_x > (PEN_MAX_WIDTH - 1)) || (pen_y > (PEN_MAX_HEIGHT - 1)))
					goto XFER_ERROR;
				pen_pressure = (uint32_t)(point_data[71] << 8) + (uint32_t)(point_data[72]);
				pen_tilt_x = (int32_t)point_data[73];
				pen_tilt_y = (int32_t)point_data[74];
				pen_distance = (uint32_t)(point_data[75] << 8) + (uint32_t)(point_data[76]);
				pen_btn1 = (uint32_t)(point_data[77] & 0x01);
				pen_btn2 = (uint32_t)((point_data[77] >> 1) & 0x01);
				pen_battery = (uint32_t)point_data[78];
//				printk("x=%d,y=%d,p=%d,tx=%d,ty=%d,d=%d,b1=%d,b2=%d,bat=%d\n", pen_x, pen_y, pen_pressure,
//						pen_tilt_x, pen_tilt_y, pen_distance, pen_btn1, pen_btn2, pen_battery);

				input_report_abs(ts->pen_input_dev, ABS_X, pen_x);
				input_report_abs(ts->pen_input_dev, ABS_Y, pen_y);
				input_report_abs(ts->pen_input_dev, ABS_PRESSURE, pen_pressure);
				input_report_key(ts->pen_input_dev, BTN_TOUCH, !!pen_pressure);
				input_report_abs(ts->pen_input_dev, ABS_TILT_X, pen_tilt_x);
				input_report_abs(ts->pen_input_dev, ABS_TILT_Y, pen_tilt_y);
				input_report_abs(ts->pen_input_dev, ABS_DISTANCE, pen_distance);
				if (!!pen_distance || !!pen_pressure) {
					input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, !!pen_distance || !!pen_pressure);
				} else {
					// if pen_distance = 0 AND pen_pressure = 0, do not report "BTN_DIGI UP" in this situation
				}
				input_report_key(ts->pen_input_dev, BTN_STYLUS, pen_btn1);
				input_report_key(ts->pen_input_dev, BTN_STYLUS2, pen_btn2);
				// TBD: pen battery event report
				// NVT_LOG("pen_battery=%d\n", pen_battery);
			} else if (pen_format_id == 0xF0) {
				// report Pen ID
			} else {
				NVT_ERR("Unknown pen format id!\n");
				goto XFER_ERROR;
			}
		} else { // pen_format_id = 0xFF, i.e. no pen present
			input_report_abs(ts->pen_input_dev, ABS_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
			input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
			input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
			input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
			input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
			input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		}

		input_sync(ts->pen_input_dev);
	} /* if (ts->pen_support) */

XFER_ERROR:

	mutex_unlock(&ts->lock);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	cpu_latency_qos_remove_request(&ts->pm_qos_req_irq);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
	return IRQ_HANDLED;
}


/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int32_t nvt_ts_check_chip_ver_trim(struct nvt_ts_hw_reg_addr_info hw_regs)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;
	uint8_t enb_casc = 0;

	/* hw reg mapping */
	ts->chip_ver_trim_addr = hw_regs.chip_ver_trim_addr;
	ts->swrst_sif_addr = hw_regs.swrst_sif_addr;
	ts->bld_spe_pups_addr = hw_regs.bld_spe_pups_addr;

	NVT_LOG("check chip ver trim with chip_ver_trim_addr=0x%06x, "
			"swrst_sif_addr=0x%06x, bld_spe_pups_addr=0x%06x\n",
			ts->chip_ver_trim_addr, ts->swrst_sif_addr, ts->bld_spe_pups_addr);

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		nvt_set_page(ts->chip_ver_trim_addr);

		buf[0] = ts->chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 7);

		buf[0] = ts->chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_READ(ts->client, buf, 7);
		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				if (trim_id_table[list].mmap->ENB_CASC_REG.addr) {
					/* check single or cascade */
					nvt_read_reg(trim_id_table[list].mmap->ENB_CASC_REG, &enb_casc);
					/* NVT_LOG("ENB_CASC=0x%02X\n", enb_casc); */
					if (enb_casc & 0x01) {
						NVT_LOG("Single Chip\n");
						ts->mmap = trim_id_table[list].mmap;
						ts->is_cascade = false;
					} else {
						NVT_LOG("Cascade Chip\n");
						ts->mmap = trim_id_table[list].mmap_casc;
						ts->is_cascade = true;
					}
				} else {
					/* for chip that do not have ENB_CASC */
					ts->mmap = trim_id_table[list].mmap;
				}
				/* hw info */
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
				ts->auto_copy = trim_id_table[list].hwinfo->auto_copy;
				ts->bld_multi_header = trim_id_table[list].hwinfo->bld_multi_header;

				/* hw reg re-mapping */
				ts->chip_ver_trim_addr = trim_id_table[list].hwinfo->hw_regs->chip_ver_trim_addr;
				ts->swrst_sif_addr = trim_id_table[list].hwinfo->hw_regs->swrst_sif_addr;
				ts->bld_spe_pups_addr = trim_id_table[list].hwinfo->hw_regs->bld_spe_pups_addr;

				NVT_LOG("set reg chip_ver_trim_addr=0x%06x, "
						"swrst_sif_addr=0x%06x, bld_spe_pups_addr=0x%06x\n",
						ts->chip_ver_trim_addr, ts->swrst_sif_addr, ts->bld_spe_pups_addr);

				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check chip version trim loop
	function. Check chip version trim via hw regs table.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int32_t nvt_ts_check_chip_ver_trim_loop(void) {
    uint8_t i = 0;
	int32_t ret = 0;

	struct nvt_ts_hw_reg_addr_info hw_regs_table[] = {
		hw_reg_addr_info_old_spe2
	};

    for (i = 0; i < (sizeof(hw_regs_table) / sizeof(struct nvt_ts_hw_reg_addr_info)); i++) {
        //---check chip version trim---
        ret = nvt_ts_check_chip_ver_trim(hw_regs_table[i]);
		if (!ret) {
			break;
		}
    }

	return ret;
}

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
void nvt_touch_resume_workqueue_callback(struct work_struct *work)
{
	NVT_LOG("start\n");
	nvt_ts_resume(&ts->client->dev);
	NVT_LOG("end\n");
}
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER) || IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
static int nvt_ts_check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return PTR_ERR(panel);
}
#endif
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 start*/
void nvt_send_gesture_flag(int gesture_command){
  	if (g_lcm_panel_id == TOUCH_SELECT_0A_CSOT){
		set_nvt_gesture_flag(!!gesture_command);
	} else if(g_lcm_panel_id == TOUCH_SELECT_0B_TIANMA){
		set_nvt_gesture_flag_tianma(!!gesture_command);
	} else if(g_lcm_panel_id == TOUCH_SELECT_0C_VISIONOX){
		set_nvt_gesture_flag_vox(!!gesture_command);
	}
}
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_COMMON)
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
static void nvt_init_touchmode_data(void)
{
	int i = 0;
	NVT_LOG("%s,ENTER\n", __func__);
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;
	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;
	/* Sensivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;
	/* Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;
        /*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 start*/
        /*Touch_Aim_Sensitivity*/
        xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = 0;
        /*Touch_Tap_Stability*/
        xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = 0;
        /*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 end*/
	/* Panel orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
	/* Edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 0;
	/*Touch_Expert_Mode setting*/
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_MIN_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][GET_CUR_VALUE] = 1;
	/* Resist RF interference*/
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][GET_CUR_VALUE] = 0;
	for (i = 0; i < Touch_Mode_NUM; i++) {
		NVT_LOG("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}
	return;
}
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
static void nvt_set_gesture_mode(int value){
	if(!ts){
		NVT_ERR("Driver not initialized");
		return ;
	}
	if(ts->ic_state <= NVT_STATE_RESUME_IN && ts->ic_state != NVT_STATE_INIT){
		ts->gesture_command_delay = value;
		NVT_LOG("nvt_gesture_command_delay = %d\n",ts->gesture_command_delay);
/*P16 code for BUGP16-4840 by xiongdejun at 2025/6/3 start*/
	}else if(ts->ic_state == NVT_STATE_RESUME_OUT || ts->ic_state == NVT_STATE_INIT){
/*P16 code for BUGP16-4840 by xiongdejun at 2025/6/3 end*/
		ts->gesture_command = value;
		NVT_LOG("nvt_gesture_command = %d\n",ts->gesture_command);
		nvt_send_gesture_flag(ts->gesture_command);
	}
}
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 start */
/*P16 code for BUGP16-4981 by xiongdejun at 2025/6/6 start*/
static void nvt_update_gesture_state(struct nvt_ts_data *ts)
{
	/* P16 code for BUGP16-3861 by liuyupei at 2025/5/29 start */
	int fodicon_status = 0;
	int gesture_command = 0;
	if (!ts) {
		NVT_ERR("Driver data is not initialized");
		return;
	}

/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 start */
	if (ts->fod_value) {
		gesture_command |= GESTURE_FOD_PRESS;
		fodicon_status = ts->fodicon_value;
        } else {
		gesture_command &= ~GESTURE_FOD_PRESS;
	}

	if (ts->aod_value || fodicon_status)
	/* P16 code for BUGP16-3861 by liuyupei at 2025/5/29 end */
		gesture_command |= GESTURE_SINGLE_TAP;
	else
		gesture_command &= ~GESTURE_SINGLE_TAP;
/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 end */
	if (ts->doubletap_value)
		gesture_command |= GESTURE_DOUBLE_TAP;
	else
		gesture_command &= ~GESTURE_DOUBLE_TAP;

	gesture_command = gesture_command & 0x07;

	NVT_LOG("gesture_command = 0x%04X",gesture_command);
	nvt_set_gesture_mode(gesture_command);

}
/*P16 code for BUGP16-4981 by xiongdejun at 2025/6/6 end*/
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 start*/
static void nvt_set_expert_mode(int nvt_value)
{
	uint8_t nvt_game_value[2] = {0};
	uint8_t ret = 0;
	/* Tolerance */
	nvt_game_value[0] = 0x70;
	nvt_game_value[1] = ts->gamemode_config[nvt_value - 1][0];
	ret = nvt_set_custom_cmd(nvt_game_value, 2);
	if (ret < 0) {
		NVT_ERR("change game mode fail");
	}
	/* Sensivity */
	nvt_game_value[0] = 0x71;
	nvt_game_value[1] = ts->gamemode_config[nvt_value - 1][1];
	ret = nvt_set_custom_cmd(nvt_game_value, 2);
	if (ret < 0) {
		NVT_ERR("change game mode fail");
	}
	/*Aim_Sensitivity*/
	nvt_game_value[0] = 0x78;
	nvt_game_value[1] = ts->gamemode_config[nvt_value - 1][2];
	ret = nvt_set_custom_cmd(nvt_game_value, 2);
	if (ret < 0) {
		NVT_ERR("change game mode fail");
	}
	/*Tap_Stability*/
	nvt_game_value[0] = 0x79;
	nvt_game_value[1] = ts->gamemode_config[nvt_value - 1][3];
	ret = nvt_set_custom_cmd(nvt_game_value, 2);
	if (ret < 0) {
		NVT_ERR("change game mode fail");
	}
}
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 end*/
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 end */
static int nvt_set_cur_value(int nvt_mode, int nvt_value)
{
	bool skip = false;
	uint8_t nvt_game_value[2] = {0};
	uint8_t temp_value = 0;
	uint8_t ret = 0;
	if (nvt_mode >= Touch_Mode_NUM || nvt_mode < 0 || !ts) {
		NVT_ERR("%s, nvt mode is error:%d or ts is Null", __func__, nvt_mode);
		return -EINVAL;
	}
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 start */
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
	if (nvt_mode == Touch_Doubletap_Mode && nvt_value >=0){
		NVT_LOG("Mode:Doubletap  Doubletap_value = %d",nvt_value);
		ts->doubletap_value = nvt_value;
		nvt_update_gesture_state(ts);
		return 0;
	}
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 start*/
	if (nvt_mode == Touch_Aod_Enable && nvt_value >=0){
		NVT_LOG("Mode:AOD  aod_value = %d",nvt_value);
		ts->aod_value = nvt_value;
		nvt_update_gesture_state(ts);
		return 0;
	}
/*P16 code for HQFEAT-88981 by liaoxianguo at 2025/4/8 end*/
	if (nvt_mode == Touch_Fod_Enable && nvt_value >=0){
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
		NVT_LOG("bTouchIsAwake = %d,Mode:FOD  fod_value = %d",bTouchIsAwake,nvt_value);
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
		ts->fod_value = nvt_value;
		nvt_update_gesture_state(ts);
		return 0;
	}
/* P16 code for HQFEAT-94432 by p-liaoxianguo at 2025/4/29 end */
/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 start */
	if (nvt_mode == Touch_FodIcon_Enable && nvt_value >=0){
		NVT_LOG("Mode:FodIcon  fodicon_value = %d",nvt_value);
		ts->fodicon_value = nvt_value;
		nvt_update_gesture_state(ts);
		return 0;
	}
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 start*/
	if (nvt_mode == Touch_Fod_Setting && nvt_value >=0){
          	/*enable 5 disable 6*/
		NVT_LOG("Mode:FodSetting  fod_setting = %d",nvt_value);
		ts->fod_setting = nvt_value;
		return 0;
	}
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 end*/
/* P16 code for BUGP16-3861 by p-liaoxianguo at 2025/5/26 end */
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 start*/
	if (nvt_mode == Touch_Expert_Mode && nvt_value >=0){
		NVT_LOG("Mode:Touch_Expert_Mode  value = %d",nvt_value);
		 if (mutex_lock_interruptible(&ts->lock)){
			 return -ERESTARTSYS;
		}
		nvt_set_expert_mode(nvt_value);
		mutex_unlock(&ts->lock);
		return 0;
	}
/*P16 code for HQFEAT-89543 by xiongdejun at 2025/6/20 end*/
/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 start*/
	if (nvt_mode == Touch_Nonui_Mode && nvt_value >= 0){
		ts->nonui_status = nvt_value;
		NVT_LOG("nonui mode is %d",nvt_value);
		return 0;
	}
/*P16 code for BUGP16-7026 by liuyupei at 2025/7/1 end*/
	xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] = nvt_value;
	if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MAX_VALUE];
	} else if (xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_MIN_VALUE];
	}
	switch (nvt_mode) {
	case Touch_Game_Mode:
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#if NVT_SUPER_RESOLUTION
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
			if (temp_value)
				current_super_resolution = 16;
			else
				current_super_resolution = 1;
			NVT_LOG("Touch_Game_Mode temp_value = %d, current_super_resolution = %d\n", temp_value, current_super_resolution);
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
			break;
	case Touch_Active_MODE:
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 start*/
	                temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE];
		        nvt_game_value[0] = 0x7A;
		        nvt_game_value[1] = temp_value;
		        NVT_LOG("Touch_Active_MODE temp_value = %d\n", temp_value);
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 end*/
			break;
	case Touch_UP_THRESHOLD:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			nvt_game_value[0] = 0x71;
			nvt_game_value[1] = temp_value;
			NVT_LOG("Touch_UP_THRESHOLD temp_value = %d\n", temp_value);
			break;
	case Touch_Tolerance:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			nvt_game_value[0] = 0x70;
			nvt_game_value[1] = temp_value;
			NVT_LOG("Touch_Tolerance temp_value = %d\n", temp_value);
			break;
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 start*/
        case Touch_Aim_Sensitivity:
                        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE];
                        nvt_game_value[0] = 0x78;
			nvt_game_value[1] = temp_value;
                        NVT_LOG("Touch_Aim_Sensitivity temp_value = %d\n", temp_value);
                        break;
        case Touch_Tap_Stability:
                        temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE];
                        nvt_game_value[0] = 0x79;
			nvt_game_value[1] = temp_value;
                        NVT_LOG("Touch_Tap_Stability temp_value = %d\n", temp_value);
                        break;
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 end*/
	case Touch_Edge_Filter:
			/* filter 0,1,2,3 = default,1,2,3 level*/
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			nvt_game_value[0] = 0x72;
			nvt_game_value[1] = temp_value;
			NVT_LOG("Touch_Edge_Filter temp_value = %d\n", temp_value);
			break;
	case Touch_Panel_Orientation:
			/* 0,1,2,3 = 0, 90, 180,270 Counter-clockwise*/
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 start*/
/*P16 code for BUGP16-566 by liuyupei at 2025/4/21 start*/
			if (temp_value == 0 || temp_value == 2) {
				nvt_game_value[0] = 0xBA;
				nvt_game_value[1] = 0x00;
			} else if (temp_value == 1) {
				nvt_game_value[0] = 0xBC;
				nvt_game_value[1] = 0x00;
			} else if (temp_value == 3) {
				nvt_game_value[0] = 0xBB;
				nvt_game_value[1] = 0x00;
			}
/*P16 code for BUGP16-566 by liuyupei at 2025/4/21 end*/
/*P16 code for HQFEAT-89543 by xiongdejun at 2024/4/9 end*/
			break;
	case Touch_Resist_RF:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Resist_RF][SET_CUR_VALUE];
			if (temp_value == 0) {
				nvt_game_value[0] = 0x76;
			} else if (temp_value == 1) {
				nvt_game_value[0] = 0x75;
			}
			nvt_game_value[1] = 0;
			break;
	default:
			/* Don't support */
			skip = true;
			break;
	};
	NVT_LOG("mode:%d, value:%d,temp_value:%d,game value:0x%x,0x%x", nvt_mode, nvt_value, temp_value, nvt_game_value[0], nvt_game_value[1]);
	if (!skip) {
		xiaomi_touch_interfaces.touch_mode[nvt_mode][GET_CUR_VALUE] =
						xiaomi_touch_interfaces.touch_mode[nvt_mode][SET_CUR_VALUE];
          	if (mutex_lock_interruptible(&ts->lock)) {
		         return -ERESTARTSYS;
	        }
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

		ret = nvt_set_custom_cmd(nvt_game_value, 2);
		if (ret < 0) {
			NVT_ERR("change game mode fail");
		}
          	mutex_unlock(&ts->lock);
	} else {
		NVT_LOG("Cmd is not support,skip!");
	}
	return 0;
}
static int nvt_get_mode_value(int mode, int value_type)
{
	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		NVT_ERR("%s, don't support\n", __func__);
	return value;
}
static int nvt_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		NVT_ERR("%s, don't support\n",  __func__);
	}
	NVT_LOG("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);
	return 0;
}
static int nvt_reset_mode(int mode)
{
	int i = 0;
	NVT_LOG("nvt_reset_mode enter\n");
	if (mode < Touch_Report_Rate && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		nvt_set_cur_value(mode, xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE]);
	} else if (mode == 0) {
		for (i = 0; i <= Touch_Report_Rate; i++) {
			if (i == Touch_Panel_Orientation) {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
			} else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
			nvt_set_cur_value(i, xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]);
		}
	} else {
		NVT_ERR("%s, don't support\n",  __func__);
	}
	NVT_LOG("%s, mode:%d\n",  __func__, mode);
	return 0;
}
#endif
/*end porting xiaomi codes*/

static int nvt_xiaomi_touch_fod_test(int value)
{
	NVT_LOG("fod test value = %d\n", value);
	if (value) {
		input_report_key(ts->input_dev, BTN_INFO, 1);
		update_fod_press_status(1);
		input_sync(ts->input_dev);
		input_mt_slot(ts->input_dev, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
		input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, 1);
		/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#if NVT_SUPER_RESOLUTION
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, 640 * SUPER_RESOLUTION_FACOTR);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, 2518 * SUPER_RESOLUTION_FACOTR);
#else
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, 640);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, 2518);
#endif
		/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
		input_sync(ts->input_dev);
	} else {
		input_mt_slot(ts->input_dev, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MINOR, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		input_report_key(ts->input_dev, BTN_INFO, 0);
		update_fod_press_status(0);
		input_sync(ts->input_dev);
	}
	return 0;
}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
static int nvt_get_charging_status(void)
{
	struct power_supply *usb_psy = NULL;
	struct power_supply *dc_psy = NULL;
	union power_supply_propval val = {0};
	int rc = 0;
	int is_charging = 0;
	is_charging = !!power_supply_is_system_supplied();
	if (!is_charging)
		return 0;
	dc_psy = power_supply_get_by_name("wireless");
	if (dc_psy) {
		rc = power_supply_get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			NVT_ERR("Couldn't get DC online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		rc = power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			NVT_ERR("Couldn't get usb online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	return 0;
}
static void charger_power_supply_work(struct work_struct *work)
{
	struct nvt_ts_data *ts_data = NULL;
	int charge_status = -1;
	if (!work) {
		NVT_ERR("work is null");
	}
	ts_data = container_of(work, struct nvt_ts_data, power_supply_work);
	charge_status = !!nvt_get_charging_status();
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 start*/
	if (charge_status != ts_data->charger_status || ts_data->charger_status <0) {
		ts_data->charger_status = charge_status;
		mutex_lock(&ts->lock);
		ts->charger_status_store = ts_data->charger_status;
		nvt_set_charger_switch(ts_data->charger_status);
		NVT_LOG("nvt charger mode is %d\n",ts_data->charger_status);
		mutex_unlock(&ts->lock);
	}
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 end*/
}
static int charger_status_event_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct nvt_ts_data *ts_data = container_of(nb, struct nvt_ts_data, charger_notifier);
	if (!ts_data)
		return 0;
	queue_work(ts_data->event_wq, &ts_data->power_supply_work);
	return 0;
}
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
#ifndef CONFIG_FACTORY_BUILD
static void nvt_reset_fw_status_func(struct work_struct *work) {
	NVT_LOG("tp callback start reset firmware status\n");
	mutex_lock(&ts->lock);
	/* Load fw quickly to reset firmware status */
	pm_wakeup_event(&ts->client->dev, 5000);
	nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, true);
	nvt_set_gesture_switch((uint8_t)(ts->gesture_command & 0xFF));
	mutex_unlock(&ts->lock);
}
#endif
void mi_display_gesture_callback(void) {
	NVT_LOG("enter mi display gesture callback\n");
#ifndef CONFIG_FACTORY_BUILD
/*P16 code for BUGP16-2429 by xiongdejun at 2025/5/30 start*/
	if(ts->gesture_command){
/*P16 code for BUGP16-2429 by xiongdejun at 2025/5/30 end*/
		queue_work(ts->nvt_reset_fw_status_wq, &ts->nvt_reset_fw_status_work);
	}
#endif
}
#ifndef CONFIG_FACTORY_BUILD
static void nvt_window_period_wait_func(struct work_struct *work)
{
	complete(&ts->nvt_window_period_completion);
	NVT_LOG("enter nvt window period wait func \n");
}
#endif
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
int tp_compare_ic(void)
{
	NVT_LOG("%s, g_lcm_panel_id = 0x%x\n", __func__, g_lcm_panel_id);
	if (g_lcm_panel_id == TOUCH_SELECT_0A_CSOT){
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 start*/
		ts->cg_type = ts->lockdowninfo[12];
		NVT_LOG("%s, CSOT cg_type is %c\n", __func__, ts->cg_type);
		if (ts->cg_type == '7'){
			BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_csot_weijing.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_csot_weijing.bin";
			NVT_LOG("weijing cg bootup success");
		} else if (ts->cg_type == '3'){
			BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_csot_ggv2.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_csot_ggv2.bin";
			NVT_LOG("ggv2 cg bootup success");
		} else {
			NVT_ERR("lockdown_info error! load default weijing.bin");
                  	BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_csot_weijing.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_csot_weijing.bin";
		}
/*P16 code for HQFEAT-93999 by xiongdejun at 2025/6/27 end*/
		NVT_LOG("TP is CSOT");
	} else if (g_lcm_panel_id == TOUCH_SELECT_0B_TIANMA) {
		BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_tm.bin";
		MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_tm.bin";
		NVT_LOG("TP is Tianma");
	} else if (g_lcm_panel_id == TOUCH_SELECT_0C_VISIONOX) {
/*P16 code for HQFEAT-102813 by xiongdejun at 2025/8/14 start*/
		ts->vox_cg_type = ts->lockdowninfo[15];
		NVT_LOG("%s, vox_cg_type is %c\n", __func__, ts->vox_cg_type);
		if(ts->vox_cg_type == '1'){
			BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_vox_weijing.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_vox_weijing.bin";
		}else if(ts->vox_cg_type == '2'){
			BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_vox_ggv2.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_vox_ggv2.bin";
		}else{
			BOOT_UPDATE_FIRMWARE_NAME = "novatek_ts_fw_vox_weijing.bin";
			MP_UPDATE_FIRMWARE_NAME = "novatek_ts_mp_vox_weijing.bin";
		}
/*P16 code for HQFEAT-102813 by xiongdejun at 2025/8/14 end*/
		NVT_LOG("TP is Visionox");
	} else {
		NVT_ERR("TP is not CORRECT!");
		return -ENODEV;
	}
	return 0;
}
/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
static void nvt_touch_dfs_test(int value){
	switch(value){
		case TOUCH_EVENT_TRANSFER_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_TRANSFER_ERR, 0, "TpTransferErr", "novatek", -1);
			break;
		case TOUCH_EVENT_FWLOAD_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_FWLOAD_ERR, 0, "TpFirmwareLoadFail", "novatek", -1);
			break;
		case TOUCH_EVENT_PARAM_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "novatek", ERROR_REGULATOR_INIT);
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "novatek", ERROR_GPIO_REQUEST);
			break;
		case TOUCH_EVENT_OPENTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_OPENTEST_FAIL, 0, "TpOpenTestFail", "novatek", -1);
			break;
		case TOUCH_EVENT_SHORTTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_SHORTTEST_FAIL, 0, "TpShortTestFail", "novatek", -1);
			break;
		default:
			NVT_ERR("don't support touch dfs test");
			break;
	}
}
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
/*******************************************************
Description:
	Novatek touchscreen driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int32_t nvt_ts_probe(struct spi_device *client)
{
	int32_t ret = 0;
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
	struct device_node *chosen = NULL;
	unsigned long size = 0;
	char *tp_lockdown_info = NULL;
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER) || IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	struct device_node *dp = NULL;
#endif
#if IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	void *cookie = NULL;
#endif
#if WAKEUP_GESTURE
	int32_t retry = 0;
#endif

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER) || IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
	dp = client->dev.of_node;

	ret = nvt_ts_check_dt(dp);
	if (ret == -EPROBE_DEFER) {
		return ret;
	}

	if (ret) {
		ret = -ENODEV;
		return ret;
	}
#endif

	NVT_LOG("start\n");

	ts = (struct nvt_ts_data *)kzalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tp_lockdown_info = (char *)of_get_property(chosen, "tp_lockdown_info", (int *)&size);
		/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
		of_property_read_u32(chosen, "lcm_panel_id", &g_lcm_panel_id);
		NVT_LOG("%s tp_lockdown_info : %s, lcm_panel_id : 0x%x\n", __func__, tp_lockdown_info, g_lcm_panel_id);
		/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
	}
	if(tp_lockdown_info){
		strlcpy(ts->lockdowninfo, tp_lockdown_info, LOCKDOWN_INFO_LENGTH);
        }
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 start*/
	for (int i = 0; i < 8; i++) {
		hex_str[0] = ts->lockdowninfo[2*i];
		hex_str[1] = ts->lockdowninfo[2*i + 1];
		hex_str[2] = '\0'; 
		sscanf(hex_str, "%2hhx", &ts->lockdown[i]);
	}
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 end*/
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
	ret = tp_compare_ic();
	if (ret < 0) {
		NVT_ERR("TP_COMPATIBLE IS NOT CORRECT!");
		return -ENODEV;
	}
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
	ts->xbuf = (uint8_t *)kzalloc(NVT_XBUF_LEN, GFP_KERNEL);
	if (ts->xbuf == NULL) {
		NVT_ERR("kzalloc for xbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_xbuf;
	}

	ts->rbuf = (uint8_t *)kzalloc(NVT_READ_LEN, GFP_KERNEL);
	if(ts->rbuf == NULL) {
		NVT_ERR("kzalloc for rbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_rbuf;
	}

#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
	ts->dev_pm_suspend = false;
	init_completion(&ts->dev_pm_resume_completion);
#endif /* NVT_PM_WAIT_BUS_RESUME_COMPLETE */

	ts->client = client;
	spi_set_drvdata(client, ts);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		NVT_ERR("Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}
	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

#ifdef SPI_CS_DELAY
	// cs setup time 300ns, cs hold time 300ns
	ts->client->cs_setup.value = 300;
	ts->client->cs_setup.unit = SPI_DELAY_UNIT_NSECS;
	ts->client->cs_hold.value = 300;
	ts->client->cs_hold.unit = SPI_DELAY_UNIT_NSECS;
	NVT_LOG("set cs_setup %d (uint %d) and cs_hold %d (uint %d)\n",
			ts->client->cs_setup.value, ts->client->cs_setup.unit,
			ts->client->cs_hold.value, ts->client->cs_hold.unit);
#endif

	ret = spi_setup(ts->client);
	if (ret < 0) {
		NVT_ERR("Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#ifdef CONFIG_MTK_SPI
    /* old usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mt_chip_conf));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

#ifdef CONFIG_SPI_MT65XX
    /* new usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mtk_chip_config));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

	NVT_LOG("mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);

	//---parse dts---
	ret = nvt_parse_dt(&client->dev);
	if (ret) {
		NVT_ERR("parse dt error\n");
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "novatek", ERROR_DTS_PARSE);
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
		goto err_spi_setup;
	}

	//---request and config GPIOs---
	ret = nvt_gpio_config(ts);
	if (ret) {
		NVT_ERR("gpio config error!\n");
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "novatek", ERROR_GPIO_REQUEST);
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
		goto err_gpio_config_failed;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	// need 10ms delay after POR(power on reset)
	msleep(10);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim_loop();
	if (ret) {
		NVT_ERR("chip is not identified\n");
		ret = -EINVAL;
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_TRANSFER_ERR, 0, "TpTransferErr", "novatek", ret);
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
		goto err_chipvertrim_failed;
	}

	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		NVT_ERR("allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

	ts->int_trigger_type = INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	//input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255
#if NVT_SUPER_RESOLUTION
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TOUCH_MAX_WIDTH * SUPER_RESOLUTION_FACOTR - 1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TOUCH_MAX_HEIGHT * SUPER_RESOLUTION_FACOTR - 1, 0, 0);
#else
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TOUCH_MAX_WIDTH - 1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TOUCH_MAX_HEIGHT - 1, 0, 0);
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
#if MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if WAKEUP_GESTURE
	for (retry = 0; retry < (sizeof(gesture_key_array) / sizeof(gesture_key_array[0])); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	input_set_capability(ts->input_dev, EV_KEY, BTN_INFO);
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_SPI;

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		NVT_ERR("register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

	if (ts->pen_support) {
		//---allocate pen input device---
		ts->pen_input_dev = input_allocate_device();
		if (ts->pen_input_dev == NULL) {
			NVT_ERR("allocate pen input device failed\n");
			ret = -ENOMEM;
			goto err_pen_input_dev_alloc_failed;
		}

		//---set pen input device info.---
		ts->pen_input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_PEN)] |= BIT_MASK(BTN_TOOL_PEN);
		//ts->pen_input_dev->keybit[BIT_WORD(BTN_TOOL_RUBBER)] |= BIT_MASK(BTN_TOOL_RUBBER);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS)] |= BIT_MASK(BTN_STYLUS);
		ts->pen_input_dev->keybit[BIT_WORD(BTN_STYLUS2)] |= BIT_MASK(BTN_STYLUS2);
		ts->pen_input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

		input_set_abs_params(ts->pen_input_dev, ABS_X, 0, PEN_MAX_WIDTH - 1, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_Y, 0, PEN_MAX_HEIGHT - 1, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_PRESSURE, 0, PEN_PRESSURE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_DISTANCE, 0, PEN_DISTANCE_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_X, PEN_TILT_MIN, PEN_TILT_MAX, 0, 0);
		input_set_abs_params(ts->pen_input_dev, ABS_TILT_Y, PEN_TILT_MIN, PEN_TILT_MAX, 0, 0);

		sprintf(ts->pen_phys, "input/pen");
		ts->pen_input_dev->name = NVT_PEN_NAME;
		ts->pen_input_dev->phys = ts->pen_phys;
		ts->pen_input_dev->id.bustype = BUS_SPI;

		//---register pen input device---
		ret = input_register_device(ts->pen_input_dev);
		if (ret) {
			NVT_ERR("register pen input device (%s) failed. ret=%d\n", ts->pen_input_dev->name, ret);
			goto err_pen_input_register_device_failed;
		}
	} /* if (ts->pen_support) */

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(ts->irq_gpio);
	if (client->irq) {
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_ts_work_func,
				ts->int_trigger_type | IRQF_ONESHOT, NVT_SPI_NAME, ts);
		if (ret != 0) {
			NVT_ERR("request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			NVT_LOG("request irq %d succeed\n", client->irq);
		}
	}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if WAKEUP_GESTURE
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
	ts->gesture_command = 0;
	ts->ic_state = NVT_STATE_INIT;
  	ts->gesture_command_delay = -1;
  	ts->fod_setting = 0;
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 start*/
	ts->fod_rpt_slot_9 = false;
	ts->fod_input_id = 0;
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 end*/
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
	device_init_wakeup(&ts->client->dev, 1);
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
	nvt_read_pos = 0;
	nvt_write_pos = 0;
	is_full = 0;
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	ts->nvt_tool_in_use = false;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	ts->fw_debug_info_switch = false;
#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = alloc_workqueue("nvt_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!nvt_fwu_wq) {
		NVT_ERR("nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(1000));
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
#endif
#if NVT_DRIVER_INSERT_FRAME
	ts->kt_delay = ktime_set(0, NVT_DRIVER_INSERT_FRAME_TIME);
	hrtimer_init(&ts->nvt_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->nvt_hrtimer.function = nvt_hrtimer_callback;
#endif
	NVT_LOG("NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_esd_check_wq) {
		NVT_ERR("nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
#ifndef CONFIG_FACTORY_BUILD
	ts->nvt_reset_fw_status_wq = create_singlethread_workqueue("nvt_reset_fw_status_wq");
	if (!ts->nvt_reset_fw_status_wq) {
		NVT_ERR("nvt_reset_fw_status_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_reset_fw_status_wq_failed;
	}
	INIT_WORK(&ts->nvt_reset_fw_status_work, nvt_reset_fw_status_func);
	ts->nvt_window_period_wait_wq = alloc_workqueue("nvt_window_period_wait_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!ts->nvt_window_period_wait_wq) {
		NVT_ERR("nvt_window_period_wait_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_window_period_wait_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_window_period_wait_work, nvt_window_period_wait_func);
	init_completion(&ts->nvt_window_period_completion);
#endif
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/

	//---set device node---
	ret = nvt_touch_sysfs_init();
	if (ret != 0) {
		NVT_ERR("nvt touch sysfs init failed. ret=%d\n", ret);
		goto err_nvt_touch_sysfs_init_failed;
	}

#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	nvt_touch_resume_workqueue = create_singlethread_workqueue("nvt_touch_resume");
	if(!nvt_touch_resume_workqueue){
		NVT_ERR("nvt resume workqueue create fail");
	}
	else{
		INIT_WORK(&nvt_touch_resume_work, nvt_touch_resume_workqueue_callback);
	}

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	ts->xiaomi_panel_notif.notifier_call = nvt_xiaomi_panel_notifier_callback;
	ret = mi_disp_register_client(&ts->xiaomi_panel_notif);
	if (ret) {
		NVT_ERR("register xiaomi_panel_notif failed. ret=%d\n", ret);
		goto err_register_xiaomi_panel_notif_failed;
	} else {
		NVT_LOG("register xiaomi_panel_notif successfully. ret=%d\n", ret);
	}
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	ts->drm_notif.notifier_call = nvt_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->drm_notif);
	if(ret) {
		NVT_ERR("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#elif IS_ENABLED(NVT_FB_NOTIFY)
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if(ret) {
		NVT_ERR("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = nvt_ts_early_suspend;
	ts->early_suspend.resume = nvt_ts_late_resume;
	ret = register_early_suspend(&ts->early_suspend);
	if(ret) {
		NVT_ERR("register early suspend failed. ret=%d\n", ret);
		goto err_register_early_suspend_failed;
	}
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	if (active_panel) {
		cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
				PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_TOUCH, active_panel,
				&nvt_panel_notifier_callback, ts);
	}
	if (!cookie) {
		NVT_ERR("register qcom panel_event_notifier failed\n");
		goto err_register_qcom_panel_event_failed;
	}
	notifier_cookie = cookie;
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
    ts->disp_notifier.notifier_call = nvt_mtk_drm_notifier_callback;
	ret = mtk_disp_notifier_register(NVT_SPI_NAME, &ts->disp_notifier);
    if (ret) {
        NVT_ERR("register mtk_disp_notifier failed\n");
        goto err_register_mtk_drm_failed;
    }
#endif
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	ts->event_wq = alloc_workqueue("nvt_charger_queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!ts->event_wq) {
		NVT_ERR("nvt-charger-queue create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_charger_queue_failed;
	}
	INIT_WORK(&ts->power_supply_work, charger_power_supply_work);
	ts->charger_notifier.notifier_call = charger_status_event_callback;
	if (power_supply_reg_notifier(&ts->charger_notifier))
	NVT_ERR("failed to register charger notifier client");
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
#if IS_ENABLED(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_COMMON)
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	xiaomi_touch_interfaces.palm_sensor_write = nvt_palm_sensor_write;
	/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
	xiaomi_touch_interfaces.getModeValue = nvt_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = nvt_set_cur_value;
	xiaomi_touch_interfaces.resetMode = nvt_reset_mode;
	xiaomi_touch_interfaces.getModeAll = nvt_get_mode_all;
	xiaomi_touch_interfaces.fod_test_store = nvt_xiaomi_touch_fod_test;
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 start*/
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 start*/
	xiaomi_touch_interfaces.panel_vendor_read = nvt_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = nvt_panel_color_read;
	xiaomi_touch_interfaces.panel_display_read = nvt_panel_display_read;
	xiaomi_touch_interfaces.touch_vendor_read = nvt_touch_vendor_read;
/*P16 code for HQFEAT-94426 by liuyupei at 2025/5/6 end*/
#if IS_ENABLED(CONFIG_MIEV)
	xiaomi_touch_interfaces.touch_dfs_test = nvt_touch_dfs_test;
#endif
/*P16 code for HQFEAT-89149 by xiongdejun at 2024/4/25 end*/
	nvt_init_touchmode_data();
	xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
#endif

#ifdef CONFIG_FACTORY_BUILD
	ts->gesture_command = 4;
	NVT_LOG("fac probe gesture_command is 4\n");
#else
	ts->gesture_command = 0;
	NVT_LOG("not fac probe, gesture_command is 0\n");
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 start*/
#if NVT_SUPER_RESOLUTION
	current_super_resolution = 1;
#endif
/*P16 code for BUGP16-6610 by P-liaoxianguo at 2025/6/24 end*/
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
	mi_display_gesture_callback_register(mi_display_gesture_callback);
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/

	bTouchIsAwake = 1;
	NVT_LOG("end\n");

	nvt_irq_enable(true);

	return 0;

#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
err_register_xiaomi_panel_notif_failed:
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
err_register_drm_notif_failed:
#elif IS_ENABLED(NVT_FB_NOTIFY)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
err_register_fb_notif_failed:
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	unregister_early_suspend(&ts->early_suspend);
err_register_early_suspend_failed:
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	if (active_panel && notifier_cookie)
		panel_event_notifier_unregister(notifier_cookie);
err_register_qcom_panel_event_failed:
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
    mtk_disp_notifier_unregister(&ts->disp_notifier);
err_register_mtk_drm_failed:
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
err_mp_proc_init_failed:
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
	nvt_touch_sysfs_deinit();
err_nvt_touch_sysfs_init_failed:
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
#ifndef CONFIG_FACTORY_BUILD
err_create_nvt_window_period_wait_wq_failed:
	if (ts->nvt_window_period_wait_wq) {
		cancel_delayed_work_sync(&ts->nvt_window_period_wait_work);
		destroy_workqueue(ts->nvt_window_period_wait_wq);
		ts->nvt_window_period_wait_wq = NULL;
	}
err_create_nvt_reset_fw_status_wq_failed:
	if (ts->nvt_reset_fw_status_wq) {
		cancel_work_sync(&ts->nvt_reset_fw_status_work);
		destroy_workqueue(ts->nvt_reset_fw_status_wq);
		ts->nvt_reset_fw_status_wq = NULL;
	}
#endif
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	if (ts->event_wq) {
		cancel_work_sync(&ts->power_supply_work);
		destroy_workqueue(ts->event_wq);
		ts->event_wq = NULL;
	}
err_create_nvt_charger_queue_failed:
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
err_create_nvt_fwu_wq_failed:
#endif
#if WAKEUP_GESTURE
	device_init_wakeup(&ts->client->dev, 0);
#endif
	free_irq(client->irq, ts);
err_int_request_failed:
	if (ts->pen_support) {
		input_unregister_device(ts->pen_input_dev);
		ts->pen_input_dev = NULL;
	}
err_pen_input_register_device_failed:
	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_free_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}
err_pen_input_dev_alloc_failed:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device_failed:
	if (ts->input_dev) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
err_chipvertrim_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
	nvt_gpio_deconfig(ts);
err_gpio_config_failed:
err_spi_setup:
err_ckeck_full_duplex:
	spi_set_drvdata(client, NULL);
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}
err_malloc_rbuf:
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
err_malloc_xbuf:
	if (ts) {
		kfree(ts);
		ts = NULL;
	}
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver release function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
#ifdef BUS_DRIVER_REMOVE_VOID_RETURN
static void nvt_ts_remove(struct spi_device *client)
#else
static int32_t nvt_ts_remove(struct spi_device *client)
#endif
{
	NVT_LOG("Removing driver...\n");
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	power_supply_unreg_notifier(&ts->charger_notifier);
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	if (nvt_touch_resume_workqueue){
		cancel_work_sync(&nvt_touch_resume_work);
		destroy_workqueue(nvt_touch_resume_workqueue);
	}
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	if (mi_disp_unregister_client(&ts->xiaomi_panel_notif))
		NVT_ERR("Error occurred while unregistering xiaomi_panel_notif.\n");
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#elif IS_ENABLED(NVT_FB_NOTIFY)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	unregister_early_suspend(&ts->early_suspend);
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	if (active_panel && notifier_cookie)
		panel_event_notifier_unregister(notifier_cookie);
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
	mtk_disp_notifier_unregister(&ts->disp_notifier);
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif
	nvt_touch_sysfs_deinit();
	/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	if (ts->event_wq) {
		cancel_work_sync(&ts->power_supply_work);
		destroy_workqueue(ts->event_wq);
		ts->event_wq = NULL;
	}
	/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */

#if NVT_DRIVER_INSERT_FRAME
	hrtimer_cancel(&ts->nvt_hrtimer);
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->client->dev, 0);
#endif

	nvt_irq_enable(false);
	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_gpio_deconfig(ts);

	if (ts->pen_support) {
		if (ts->pen_input_dev) {
			input_unregister_device(ts->pen_input_dev);
			ts->pen_input_dev = NULL;
		}
	}

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

	spi_set_drvdata(client, NULL);

	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}

	if (ts) {
		kfree(ts);
		ts = NULL;
	}

#ifdef BUS_DRIVER_REMOVE_VOID_RETURN
	return;
#else
	return 0;
#endif
}

static void nvt_ts_shutdown(struct spi_device *client)
{
	NVT_LOG("Shutdown driver...\n");

	nvt_irq_enable(false);
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	power_supply_unreg_notifier(&ts->charger_notifier);
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	if (mi_disp_unregister_client(&ts->xiaomi_panel_notif))
		NVT_ERR("Error occurred while unregistering xiaomi_panel_notif.\n");
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
	if (msm_drm_unregister_client(&ts->drm_notif))
		NVT_ERR("Error occurred while unregistering drm_notifier.\n");
#elif IS_ENABLED(NVT_FB_NOTIFY)
	if (fb_unregister_client(&ts->fb_notif))
		NVT_ERR("Error occurred while unregistering fb_notifier.\n");
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
	unregister_early_suspend(&ts->early_suspend);
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
	if (active_panel && notifier_cookie)
		panel_event_notifier_unregister(notifier_cookie);
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
	mtk_disp_notifier_unregister(&ts->disp_notifier);
#endif

#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif
	nvt_touch_sysfs_deinit();

/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 start*/
#ifndef CONFIG_FACTORY_BUILD
	if (ts->nvt_window_period_wait_wq) {
		cancel_delayed_work_sync(&ts->nvt_window_period_wait_work);
		destroy_workqueue(ts->nvt_window_period_wait_wq);
		ts->nvt_window_period_wait_wq = NULL;
	}
	if (ts->nvt_reset_fw_status_wq) {
		cancel_work_sync(&ts->nvt_reset_fw_status_work);
		destroy_workqueue(ts->nvt_reset_fw_status_wq);
		ts->nvt_reset_fw_status_wq = NULL;
	}
#endif
/*P16 code for HQFEAT-89614 by liaoxianguo at 2025/4/3 end*/
	/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	if (ts->event_wq) {
		cancel_work_sync(&ts->power_supply_work);
		destroy_workqueue(ts->event_wq);
		ts->event_wq = NULL;
	}
	/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */

#if NVT_DRIVER_INSERT_FRAME
	hrtimer_cancel(&ts->nvt_hrtimer);
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if WAKEUP_GESTURE
	device_init_wakeup(&ts->client->dev, 0);
#endif
}

/*******************************************************
Description:
	Novatek touchscreen driver suspend function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_suspend(struct device *dev)
{
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	ts->nvt_tool_in_use = false;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 start*/
	ts->fod_rpt_slot_9 = false;
	ts->fod_input_id = 0;
/*P16 code for BUGP16-7431 by liuyupei at 2025/7/8 end*/
	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}
	ts->ic_state = NVT_STATE_SUSPEND_IN;
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 start*/
	if ((ts->gesture_command & GESTURE_FOD_PRESS) == 0 && ts->fod_setting == 5) {
		ts->gesture_command |= GESTURE_FOD_PRESS;
          	if(ts->fodicon_value){
                  	ts->gesture_command |= GESTURE_SINGLE_TAP;
                }
          	nvt_send_gesture_flag(ts->gesture_command);
		NVT_LOG("fod_setting enable, gesture_command switch to:0x%02x\n", ts->gesture_command);
	}
/*P16 code for BUGP16-5788 by xiongdejun at 2025/6/18 end*/
#ifdef CONFIG_FACTORY_BUILD
	ts->gesture_command = 0;
	NVT_LOG("fac suspend gesture_command is 0\n");
#endif

#if WAKEUP_GESTURE
	if (ts->gesture_command == false) {
		nvt_irq_enable(false);
	}
#else
	nvt_irq_enable(false);
#endif

#if NVT_TOUCH_ESD_PROTECT
	NVT_LOG("cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	mutex_lock(&ts->lock);

	NVT_LOG("start, gesture_command=0x%04X\n", ts->gesture_command);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	update_palm_sensor_value(0);
	if (ts->palm_sensor_switch) {
		NVT_LOG("palm sensor on status, switch to off\n");
		nvt_set_pocket_palm_switch(false);
		ts->palm_sensor_switch = false;
	}
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
	/* gesture mode setup */
/*P16 code for BUGP16-1599 by xiongdejun at 2025/5/13 start*/
	if (ts->gesture_command) {
/*P16 code for BUGP16-1599 by xiongdejun at 2025/5/13 end*/
		nvt_enable_gesture_mode(true);
/*P16 code for BUGP16-420 by xiongdejun at 2025/5/16 start*/
		if (enable_irq_wake(ts->client->irq)){
			NVT_ERR("enable_irq_wake(irq:%d) fail", ts->client->irq);
		}
/*P16 code for BUGP16-420 by xiongdejun at 2025/5/16 end*/
	} else {
		nvt_enable_gesture_mode(false);
	}
	/* gesture mode setup end */

/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
	if(ts->ic_state == NVT_STATE_SUSPEND_IN){
		ts->ic_state = NVT_STATE_SUSPEND_OUT;
	}
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
	bTouchIsAwake = 0;

	mutex_unlock(&ts->lock);

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		//input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
#if NVT_DRIVER_INSERT_FRAME
		ts->input_event_state[i] = 0;
#endif
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	/* release pen event */
	if (ts->pen_support) {
		input_report_abs(ts->pen_input_dev, ABS_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_PRESSURE, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_X, 0);
		input_report_abs(ts->pen_input_dev, ABS_TILT_Y, 0);
		input_report_abs(ts->pen_input_dev, ABS_DISTANCE, 0);
		input_report_key(ts->pen_input_dev, BTN_TOUCH, 0);
		input_report_key(ts->pen_input_dev, BTN_TOOL_PEN, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS, 0);
		input_report_key(ts->pen_input_dev, BTN_STYLUS2, 0);
		input_sync(ts->pen_input_dev);
	}

	msleep(50);
/*P16 code for BUGP16-5004 by xiongdejun at 2025/6/20 start*/
	if(ts->fod_finger) {
		ts->fod_finger = false;
		nvt_ts_fod_up_report();
		NVT_LOG("fod up for suspend. \n");
	}
/*P16 code for BUGP16-5004 by xiongdejun at 2025/6/20 end*/
	NVT_LOG("end\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen driver resume function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_resume(struct device *dev)
{
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 start*/
	ts->nvt_tool_in_use = false;
/*P16 code for BUGP16-584 by xiongdejun at 2025/5/27 end*/
	if (bTouchIsAwake) {
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
		mutex_lock(&ts->lock);
		nvt_set_edge_reject_switch(edge_orientation_store);
		mutex_unlock(&ts->lock);
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
		NVT_LOG("Touch is already resume\n");
		return 0;
	}
	ts->ic_state = NVT_STATE_RESUME_IN;
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	NVT_LOG("start, gesture_command:0x%02x, fod_finger: %d\n", ts->gesture_command, ts->fod_finger);

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	if (ts->gesture_command) {
		NVT_LOG("skip download firmware\n");
		nvt_check_fw_reset_state(RESET_STATE_REK);
		nvt_set_gesture_switch(0x00);
	} else {
		if (nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME, false)) {
			NVT_ERR("download firmware failed, ignore check fw state\n");
		} else {
			nvt_check_fw_reset_state(RESET_STATE_REK);
		}
	}
#if WAKEUP_GESTURE
	if (ts->gesture_command == 0) {
		nvt_irq_enable(true);
	}
#else
	nvt_irq_enable(true);
#endif

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	bTouchIsAwake = 1;
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 start*/
	if(ts->ic_state == NVT_STATE_RESUME_IN){
		ts->ic_state = NVT_STATE_RESUME_OUT;
	}
	if(ts->gesture_command_delay >= 0){
		nvt_set_gesture_mode(ts->gesture_command_delay);
		ts->gesture_command_delay = -1;
	}
/*P16 code for BUGP16-2768 by xiongdejun at 2025/5/26 end*/
#ifdef CONFIG_FACTORY_BUILD
	ts->gesture_command = 4;
	NVT_LOG("fac resume gesture_command is 4\n");
#endif
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
	nvt_set_edge_reject_switch(edge_orientation_store);
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
	mutex_unlock(&ts->lock);
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	mutex_lock(&ts->lock);
	NVT_LOG("palm_sensor_switch=%d", ts->palm_sensor_switch);
	if (ts->palm_sensor_switch) {
		NVT_LOG("palm sensor on status, switch to on\n");
		nvt_set_pocket_palm_switch(true);
	}
	mutex_unlock(&ts->lock);
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 start*/
	mutex_lock(&ts->lock);
	nvt_set_charger_switch(ts->charger_status_store);
	NVT_LOG("nvt charger mode is %d in resume\n",ts->charger_status_store);
	mutex_unlock(&ts->lock);
/*P16 code for BUGP16-3227 by p-liaoxianguo at 2025/6/4 end*/
	NVT_LOG("end\n");

	return 0;
}

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
static int nvt_xiaomi_panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct mi_disp_notifier *evdata = data;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, xiaomi_panel_notif);
	int blank = 0;
	int ret = 0;
	NVT_LOG("nvt_xiaomi_panel_notifier_callback IN");
	if (!(event == MI_DISP_DPMS_EARLY_EVENT ||
		event == MI_DISP_DPMS_EVENT)) {
		NVT_LOG("event(%lu) do not need process", event);
		return 0;
	}
	if (evdata && evdata->data && ts) {
/*P16 code for BUGP16-5761 by xiongdejun at 2025/6/17 start*/
		flush_workqueue(nvt_touch_resume_workqueue);
/*P16 code for BUGP16-5761 by xiongdejun at 2025/6/17 end*/
		blank = *(int *)(evdata->data);
		NVT_LOG("notifier tp event:%lu, code:%d.", event, blank);
		if (event == MI_DISP_DPMS_EARLY_EVENT
			&& (blank == MI_DISP_DPMS_POWERDOWN
			|| blank == MI_DISP_DPMS_LP1
			|| blank == MI_DISP_DPMS_LP2)) {
			NVT_LOG("event:%lu,blank:%d", event, blank);
			nvt_ts_suspend(&ts->client->dev);
		} else if (event == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {
			NVT_LOG("touchpanel resume, event:%lu,blank:%d", event, blank);
			ret = queue_work(nvt_touch_resume_workqueue, &nvt_touch_resume_work);
			if (!ret){
				NVT_ERR("failed to queue resume work\n");
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#elif IS_ENABLED(NVT_MSM_DRM_NOTIFY)
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_suspend(&ts->client->dev);
			}
		} else if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}
#elif IS_ENABLED(NVT_FB_NOTIFY)
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_suspend(&ts->client->dev);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#elif IS_ENABLED(NVT_EARLYSUSPEND_NOTIFY)
/*******************************************************
Description:
	Novatek touchscreen driver early suspend function.

return:
	n.a.
*******************************************************/
static void nvt_ts_early_suspend(struct early_suspend *h)
{
	nvt_ts_suspend(ts->client, PMSG_SUSPEND);
}

/*******************************************************
Description:
	Novatek touchscreen driver late resume function.

return:
	n.a.
*******************************************************/
static void nvt_ts_late_resume(struct early_suspend *h)
{
	nvt_ts_resume(ts->client);
}
#elif IS_ENABLED(NVT_QCOM_PANEL_EVENT_NOTIFY)
static void nvt_panel_notifier_callback(enum panel_event_notifier_tag tag,
		struct panel_event_notification *notification, void *client_data)
{
	struct nvt_ts_data *ts = client_data;

	if (!notification) {
		NVT_ERR("Invalid notification\n");
		return;
	}

	NVT_LOG("Notification type:%d, early_trigger:%d",
			notification->notif_type,
			notification->notif_data.early_trigger);

	switch (notification->notif_type) {
		case DRM_PANEL_EVENT_UNBLANK:
			if (notification->notif_data.early_trigger)
				NVT_LOG("resume notification pre commit\n");
			else
				nvt_ts_resume(&ts->client->dev);
			break;

		case DRM_PANEL_EVENT_BLANK:
			if (notification->notif_data.early_trigger)
				nvt_ts_suspend(&ts->client->dev);
			else
				NVT_LOG("suspend notification post commit\n");
			break;

		case DRM_PANEL_EVENT_BLANK_LP:
			NVT_LOG("received lp event\n");
			break;

		case DRM_PANEL_EVENT_FPS_CHANGE:
			NVT_LOG("Received fps change old fps:%d new fps:%d\n",
					notification->notif_data.old_fps,
					notification->notif_data.new_fps);
			break;
		default:
			NVT_LOG("notification serviced :%d\n",
					notification->notif_type);
			break;
	}
}
#elif IS_ENABLED(NVT_MTK_DRM_NOTIFY)
static int nvt_mtk_drm_notifier_callback(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int *blank = (int *)data;

	if (!blank) {
		NVT_ERR("Invalid blank\n");
		return -1;
	}

	if (event == MTK_DISP_EARLY_EVENT_BLANK) {
		if (*blank == MTK_DISP_BLANK_POWERDOWN) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_suspend(&ts->client->dev);
		}
	} else if (event == MTK_DISP_EVENT_BLANK) {
		if (*blank == MTK_DISP_BLANK_UNBLANK) {
			NVT_LOG("event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#endif

#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
static int nvt_ts_pm_suspend(struct device *dev)
{
	ts->dev_pm_suspend = true;
	reinit_completion(&ts->dev_pm_resume_completion);

	return 0;
}

static int nvt_ts_pm_resume(struct device *dev)
{
	ts->dev_pm_suspend = false;
	complete(&ts->dev_pm_resume_completion);

	return 0;
}

static const struct dev_pm_ops nvt_ts_dev_pm_ops = {
	.suspend = nvt_ts_pm_suspend,
	.resume  = nvt_ts_pm_resume,
};
#endif /* NVT_PM_WAIT_BUS_RESUME_COMPLETE */

static const struct spi_device_id nvt_ts_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id nvt_match_table[] = {
	{ .compatible = "novatek,NVT-ts-spi",},
	{ },
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
#if NVT_PM_WAIT_BUS_RESUME_COMPLETE
		.pm = &nvt_ts_dev_pm_ops,
#endif /* NVT_PM_WAIT_BUS_RESUME_COMPLETE */
	},
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	NVT_LOG("start\n");

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		NVT_ERR("failed to add spi driver");
		goto err_driver;
	}

	NVT_LOG("finished\n");

err_driver:
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit nvt_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
late_initcall(nvt_driver_init);
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
#else
module_init(nvt_driver_init);
#endif
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
