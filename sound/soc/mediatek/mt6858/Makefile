# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include

# spm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# pbm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm

# common include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

# VIP include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/eas
subdir-ccflags-y += -I$(srctree)/kernel/sched/
