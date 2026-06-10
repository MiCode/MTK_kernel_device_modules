/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _SIPA_COMMOMN_H
#define _SIPA_COMMOMN_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/version.h>


#define SIPA_DRIVER_VERSION					("3.1.16-251231")
#define SIPA_MAX_CHANNEL_SUPPORT			(8)
#define SIPA_FW_BIN							("sipa.bin")

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
#define SIPA_KERNEL_VER_OVER_4_16_28
#endif

#ifdef SIPA_KERNEL_VER_OVER_4_16_28
typedef struct snd_soc_component sipa_snd_soc_codec_t;
typedef const struct snd_soc_component_driver sipa_snd_soc_codec_driver_t;
#else
typedef struct snd_soc_codec sipa_snd_soc_codec_t;
typedef const struct snd_soc_codec_driver sipa_snd_soc_codec_driver_t;
#endif

#define SIPA_NAME_BUF_MAX (50)
#define SIPA_KCTL_TYPE_REG_DEFINE		0
#define SIPA_CREAT_CONTRILS_MODE        0

enum {
	KCTL_TYPE_POWER = 0,
	KCTL_TYPE_SCENE,
	KCTL_TYPE_LIMIT,
	KCTL_TYPE_STATUS,
	KCTL_TYPE_TYPE,
	KCTL_TYPE_MUTE,
#if SIPA_KCTL_TYPE_REG_DEFINE
	KCTL_TYPE_REG,
#endif
	SIPA_KCTL_NUM,
};

struct sipa_err {
	unsigned long owi_set_mode_cnt;
	unsigned long owi_set_mode_err_cnt;
	unsigned long owi_write_err_cnt;
	unsigned long owi_polarity_err_cnt;
	unsigned long owi_max_retry_cnt;
	unsigned long owi_max_gap;
	unsigned long owi_max_deviation;
	unsigned long owi_write_data_err_cnt;
	unsigned long owi_write_data_cnt;
};

enum {
	AUDIO_SCENE_PLAYBACK = 0,
	AUDIO_SCENE_VOICE,
	AUDIO_SCENE_VOIP,
	AUDIO_SCENE_RECEIVER,
	AUDIO_SCENE_FACTORY,
	AUDIO_SCENE_FM,
	AUDIO_SCENE_NUM
};

typedef struct sipa_dev_s {
	char name[32];
	unsigned int chip_type;
	struct platform_device *pdev;
	struct i2c_client *client;
	struct workqueue_struct *sipa_wq;
	struct delayed_work interrupt_work;
	struct delayed_work fw_load_work;
	int disable_pin;
	int rst_pin;
	int owi_pin;
	int irq_pin;
	int id_pin;

	unsigned int owi_delay_us;
	unsigned int owi_cur_mode[AUDIO_SCENE_NUM];
	unsigned int owi_polarity;

	spinlock_t rst_lock;
	spinlock_t owi_lock;

	struct regmap *regmap;
	unsigned int scene;

	uint32_t channel_num;
	uint32_t en_compatible_type;
	uint32_t dyn_ud_vdd_port;
	uint32_t en_dyn_id;
	// uint32_t en_dyn_ud_vdd;
	// uint32_t en_dyn_ud_pvdd;
	// uint32_t en_spk_cal_dl;
	// uint32_t spk_model_flag;
	uint8_t  pa_status;
	uint8_t  fw_load_count;

	sipa_snd_soc_codec_t *codec;
	int pstream;
	int cstream;
	bool mute;

	struct sipa_err err_info;
	bool power_mode;

	int spksw_gpio;
	int spksw_level;
	bool ftm_mode;
	int gain;
} sipa_dev_t;

struct sipa_chip_compat {
	const uint32_t sub_type;
	struct {
		const uint32_t *chips;
		const uint32_t num;
	};
};

enum TFA_DEVICE_MUTE {
	SIPA_DEVICE_MUTE_OFF = 0,
	SIPA_DEVICE_MUTE_ON,
};

enum {
	SIPA_CHANNEL_0 = 0,
	SIPA_CHANNEL_1,
	SIPA_CHANNEL_2,
	SIPA_CHANNEL_3,
#if 0
	SIPA_CHANNEL_4,
	SIPA_CHANNEL_5,
	SIPA_CHANNEL_6,
	SIPA_CHANNEL_7,
#endif
	SIPA_CHANNEL_NUM
};

enum {
	CHIP_TYPE_SIA8101 = 0,
	CHIP_TYPE_SIA8109,
	CHIP_TYPE_SIA8152,
	CHIP_TYPE_SIA8152S,
	CHIP_TYPE_SIA8159,
	CHIP_TYPE_SIA8159A,
	// add analog chip type here
	CHIP_TYPE_SIA9195,
	CHIP_TYPE_SIA9196,
	CHIP_TYPE_SIA9175,
	// add digital chip type here
	CHIP_TYPE_SIA8001,
	CHIP_TYPE_SIA8102,
	CHIP_TYPE_SIA9177,
	CHIP_TYPE_SIA9255,
	// new add
	CHIP_TYPE_SIA8100X,
	CHIP_TYPE_SIA81X9,
	CHIP_TYPE_SIA8152X,
	CHIP_TYPE_SIA917X,
	CHIP_TYPE_SIA8150,
	CHIP_TYPE_SIA815T,
	CHIP_TYPE_SIA9187,
	CHIP_TYPE_SIA5118,
	CHIP_TYPE_SIA9197X,
	CHIP_TYPE_SIA8132,
	CHIP_TYPE_SIA5118S = 23,
	CHIP_TYPE_SIA5118X = 24,
	CHIP_TYPE_SIA9306 = 25,
	// add compatible chip type here
	CHIP_TYPE_UNKNOWN = 200,
	CHIP_TYPE_INVALID
};

#define IS_DIGITAL_PA_TYPE(type) \
			((type == CHIP_TYPE_SIA9195 || \
			 type == CHIP_TYPE_SIA9196 || \
			 type == CHIP_TYPE_SIA9175 || \
			 type == CHIP_TYPE_SIA9255 || \
			 type == CHIP_TYPE_SIA9177 || \
			 type == CHIP_TYPE_SIA9187 || \
			 type == CHIP_TYPE_SIA5118 || \
			 type == CHIP_TYPE_SIA5118S || \
			 type == CHIP_TYPE_SIA5118X || \
			 type == CHIP_TYPE_SIA9197X || \
			 type == CHIP_TYPE_SIA917X || \
			 type == CHIP_TYPE_SIA9306 )  \
			 ? true \
			 : false)

#define IS_SUPPORT_OWI_TYPE(type) \
			((type == CHIP_TYPE_SIA8001  || \
			 type == CHIP_TYPE_SIA8102  || \
			 type == CHIP_TYPE_SIA8100X )  \
			 ? true \
			 : false)

 //5118 power up need pull up rst.
#define IS_NEED_PULL_RST_TYPE(type) \
			((type == CHIP_TYPE_SIA81X9  || \
			 type == CHIP_TYPE_SIA8109  || \
			 type == CHIP_TYPE_SIA815T  || \
			 type == CHIP_TYPE_SIA5118  || \
			 type == CHIP_TYPE_SIA5118S || \
			 type == CHIP_TYPE_SIA5118X || \
			 type == CHIP_TYPE_SIA9197X )   \
			 ? true \
			 : false) 

#define IS_DIGITAL_PA_PULL_RST_TYPE(type)  \
			((type == CHIP_TYPE_SIA5118  || \
			 type == CHIP_TYPE_SIA5118S || \
			 type == CHIP_TYPE_SIA5118X || \
			 type == CHIP_TYPE_SIA9197X )   \
			 ? true \
			 : false) 

#define SIPA_MAX_REG_ADDR					(0xFF)

#define SIA81XX_REG_R_O						(0x00000001)
#define SIA81XX_REG_W_O						(0x00000001 << 1)
#define SIA81XX_REG_RW						(SIA81XX_REG_R_O | SIA81XX_REG_W_O)

#define SIA9187_REG_SPKGAIN                 (0x1B)
#define SIA9187_REG_PWMGAIN                 (0x17)
#define SIA9187_REG_INPUTGIN                (0x14)
#define SIA9187_SPKGAIN_MIX                 (7)
#define SIA9187_SPKGAIN_MAX                 (22)
#define SIA9187_GAIN_MIX                    (0)
#define SIA9187_GAIN_MAX                    (22)

/* error list */
/* pulse width time out */
#define EPTOUT								(100)
/* pulse electrical level opposite with the polarity */
#define EPOLAR								(101)
#define EEXEC								(102)
#define EOUTR								(103)

sipa_dev_t *find_sipa_dev(struct device_node *of_node);

int mievent_report(unsigned int eventid,const char *value,struct device *dev);

int sipa_vol_config_regmap(sipa_dev_t *si_pa);

#endif /* _SIPA_COMMOMN_H */
