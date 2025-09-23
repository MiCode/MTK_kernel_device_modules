# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6853-afe-objs := \
	mt6853-afe-pcm.o \
	mt6853-afe-clk.o \
	mt6853-afe-gpio.o \
	mt6853-dai-adda.o \
	mt6853-afe-control.o \
	mt6853-dai-i2s.o \
	mt6853-dai-hw-gain.o \
	mt6853-dai-src.o \
	mt6853-dai-pcm.o \
	mt6853-dai-tdm.o \
	mt6853-dai-hostless.o \
	mt6853-misc-control.o

obj-$(CONFIG_SND_SOC_MT6853) += snd-soc-mt6853-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6853_MT6359P) += mt6853-mt6359p.o
# MENU Governor Predict(low power)
ifneq ($(CONFIG_MTK_ACAO_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ifneq ($(CONFIG_MTK_AUDIODSP_SUPPORT), )
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
endif

# spm
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include